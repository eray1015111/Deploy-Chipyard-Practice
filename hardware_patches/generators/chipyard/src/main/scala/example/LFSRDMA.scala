package chipyard.example

import chisel3._
import chisel3.util._
import freechips.rocketchip.subsystem.{BaseSubsystem, FBUS, PBUS}
import freechips.rocketchip.diplomacy._
import freechips.rocketchip.prci._
import freechips.rocketchip.regmapper.{HasRegMap, RegField}
import freechips.rocketchip.tilelink._
import org.chipsalliance.cde.config.{Parameters, Field, Config}

case class LFSRDMAParams(
  address: BigInt = 0x7000
)

case object LFSRDMAKey extends Field[Option[LFSRDMAParams]](None)

class LFSRDMA(params: LFSRDMAParams, beatBytes: Int)(implicit p: Parameters) extends ClockSinkDomain(ClockSinkParameters())(p) {
  val device = new SimpleDevice("lfsr-dma", Seq("custom,lfsr-dma"))
  val controlNode = TLRegisterNode(Seq(AddressSet(params.address, 4096-1)), device, "reg/control", beatBytes=beatBytes)
  val masterNode = TLClientNode(Seq(TLMasterPortParameters.v1(Seq(TLClientParameters(
    name = "lfsr-dma", sourceId = IdRange(0, 1))))))

  override lazy val module = new LFSRDMAImpl
  class LFSRDMAImpl extends Impl {
    withClockAndReset(clock, reset) {
      val (mem, edge) = masterNode.out(0)
      
      val s_idle :: s_read_req :: s_read_resp :: s_compute :: s_write_req :: s_write_resp :: s_done :: Nil = Enum(7)
      val state = RegInit(s_idle)

      val dma_addr = Reg(UInt(64.W))
      val steps = Reg(UInt(32.W))
      val seed = Reg(UInt(32.W))

      // Slave MMIO port integration
      val status = Cat(state === s_idle, state === s_done) // ready, valid

      val steps_input = Wire(new DecoupledIO(UInt(32.W)))
      steps_input.ready := state === s_idle
      when (steps_input.fire) {
        steps := steps_input.bits
        state := s_read_req
      }

      val done_read = Wire(new DecoupledIO(UInt(32.W)))
      done_read.bits := seed
      done_read.valid := state === s_done
      when (done_read.fire) {
        state := s_idle
      }

      controlNode.regmap(
        0x00 -> Seq(RegField.r(2, status)),
        0x08 -> Seq(RegField.w(64, dma_addr)),
        0x10 -> Seq(RegField.w(32, steps_input)),
        0x14 -> Seq(RegField.r(32, done_read))
      )

      // Master DMA port integration
      mem.a.valid := (state === s_read_req) || (state === s_write_req)
      
      val read_a = edge.Get(
        fromSource = 0.U,
        toAddress = dma_addr,
        lgSize = 2.U)._2 // 4 bytes

      val write_a = edge.Put(
        fromSource = 0.U,
        toAddress = dma_addr + 4.U,
        lgSize = 2.U,
        data = seed)._2

      mem.a.bits := Mux(state === s_read_req, read_a, write_a)
      mem.d.ready := (state === s_read_resp) || (state === s_write_resp)

      when (state === s_read_req && mem.a.fire) {
        state := s_read_resp
      }
      when (state === s_read_resp && mem.d.fire) {
        seed := mem.d.bits.data(31, 0)
        state := Mux(steps === 0.U, s_write_req, s_compute)
      }

      // LFSR Compute
      when (state === s_compute) {
        seed := Mux(seed(0), (seed >> 1) ^ 0x80200003L.U, seed >> 1)
        steps := steps - 1.U
        when (steps === 1.U) {
          state := s_write_req
        }
      }

      when (state === s_write_req && mem.a.fire) {
        state := s_write_resp
      }
      when (state === s_write_resp && mem.d.fire) {
        state := s_done
      }
    }
  }
}

trait CanHavePeripheryLFSRDMA { this: BaseSubsystem =>
  private val portName = "lfsr-dma"
  private val pbus = locateTLBusWrapper(PBUS)
  private val fbus = locateTLBusWrapper(FBUS)

  p(LFSRDMAKey) match {
    case Some(params) => {
      val dma = LazyModule(new LFSRDMA(params, pbus.beatBytes)(p))
      dma.clockNode := pbus.fixedClockNode
      pbus.coupleTo(portName) {
        TLInwardClockCrossingHelper("dma_control", dma, dma.controlNode)(SynchronousCrossing()) :=
        TLFragmenter(pbus.beatBytes, pbus.blockBytes) := _
      }
      fbus.coupleFrom(portName) { _ := dma.masterNode }
    }
    case None => None
  }
}

class WithLFSRDMA extends Config((site, here, up) => {
  case LFSRDMAKey => Some(LFSRDMAParams())
})
