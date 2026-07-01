package chipyard.example

import chisel3._
import chisel3.util._
import freechips.rocketchip.diplomacy._
import freechips.rocketchip.prci._
import freechips.rocketchip.regmapper.RegField
import freechips.rocketchip.subsystem.{BaseSubsystem, FBUS, PBUS}
import freechips.rocketchip.tilelink._
import org.chipsalliance.cde.config.{Config, Field, Parameters}

case class LFSRDMAParams(
  address: BigInt = 0x7000)

case object LFSRDMAKey extends Field[Option[LFSRDMAParams]](None)

class LFSRDMA(params: LFSRDMAParams, beatBytes: Int)(implicit p: Parameters)
    extends ClockSinkDomain(ClockSinkParameters())(p) {
  val device = new SimpleDevice("lfsr-dma", Seq("custom,lfsr-dma"))
  val controlNode = TLRegisterNode(
    Seq(AddressSet(params.address, 4096 - 1)),
    device,
    "reg/control",
    beatBytes = beatBytes)
  val masterNode = TLClientNode(Seq(TLMasterPortParameters.v1(Seq(TLClientParameters(
    name = "lfsr-dma",
    sourceId = IdRange(0, 1))))))

  lazy val module = new LFSRDMAImpl

  class LFSRDMAImpl extends Impl {
    withClockAndReset(clock, reset) {
      val (mem, edge) = masterNode.out(0)

      val sIdle :: sReadReq :: sReadResp :: sCompute :: sWriteReq :: sWriteResp :: sDone :: Nil = Enum(7)
      val state = RegInit(sIdle)
      val dmaAddr = Reg(UInt(64.W))
      val steps = Reg(UInt(32.W))
      val seed = Reg(UInt(32.W))

      val start = Wire(Decoupled(UInt(32.W)))
      val done = Wire(Decoupled(UInt(32.W)))
      val status = Cat(state === sIdle, state === sDone)

      start.ready := state === sIdle
      when(start.fire) {
        steps := start.bits
        state := sReadReq
      }

      done.valid := state === sDone
      done.bits := seed
      when(done.fire) {
        state := sIdle
      }

      node.regmap(
        0x00 -> Seq(RegField.r(2, status)),
        0x08 -> Seq(RegField.w(64, dmaAddr)),
        0x10 -> Seq(RegField.w(32, start)),
        0x18 -> Seq(RegField.r(32, done)))

      val (_, get) = edge.Get(0.U, dmaAddr, 2.U)
      val (_, put) = edge.Put(0.U, dmaAddr, 2.U, seed)

      mem.a.valid := state === sReadReq || state === sWriteReq
      mem.a.bits := Mux(state === sWriteReq, put, get)
      mem.d.ready := true.B

      when(state === sReadReq && mem.a.fire) {
        state := sReadResp
      }

      when(state === sReadResp && mem.d.fire) {
        seed := mem.d.bits.data(31, 0)
        state := Mux(steps === 0.U, sWriteReq, sCompute)
      }

      when(state === sCompute) {
        seed := Mux(seed(0), (seed >> 1) ^ 0x80200003L.U(32.W), seed >> 1)
        steps := steps - 1.U
        when(steps === 1.U) {
          state := sWriteReq
        }
      }

      when(state === sWriteReq && mem.a.fire) {
        state := sWriteResp
      }

      when(state === sWriteResp && mem.d.fire) {
        state := sDone
      }
    }
  }
}

trait CanHavePeripheryLFSRDMA { this: BaseSubsystem =>
  private val portName = "lfsr-dma"
  private val pbus = locateTLBusWrapper(PBUS)
  private val fbus = locateTLBusWrapper(FBUS)

  p(LFSRDMAKey) match {
    case Some(params) =>
      val dma = LazyModule(new LFSRDMA(params, pbus.beatBytes)(p))
      dma.clockNode := pbus.fixedClockNode
      pbus.coupleTo(portName) {
        TLInwardClockCrossingHelper("dma_control", dma, dma.controlNode)(SynchronousCrossing()) :=
          TLFragmenter(pbus.beatBytes, pbus.blockBytes) := _
      }
      fbus.coupleFrom(portName) { _ := dma.masterNode }
    case None =>
  }
}

class WithLFSRDMA extends Config((site, here, up) => {
  case LFSRDMAKey => Some(LFSRDMAParams())
})
