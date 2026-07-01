package chipyard.example

import chisel3._
import chisel3.experimental.IntParam
import chisel3.util._
import freechips.rocketchip.diplomacy._
import freechips.rocketchip.prci._
import freechips.rocketchip.regmapper.RegField
import freechips.rocketchip.subsystem.{BaseSubsystem, PBUS}
import freechips.rocketchip.tilelink._
import org.chipsalliance.cde.config.{Config, Field, Parameters}

case class LFSRParams(
  address: BigInt = 0x6000,
  width: Int = 32)

case object LFSRKey extends Field[Option[LFSRParams]](None)

class LFSRIO(val w: Int) extends Bundle {
  val clock = Input(Clock())
  val reset = Input(Bool())
  val input_ready = Output(Bool())
  val input_valid = Input(Bool())
  val seed = Input(UInt(w.W))
  val steps = Input(UInt(w.W))
  val output_ready = Input(Bool())
  val output_valid = Output(Bool())
  val lfsr_result = Output(UInt(w.W))
  val busy = Output(Bool())
}

class LFSRTopIO extends Bundle {
  val lfsr_busy = Output(Bool())
}

trait HasLFSRTopIO {
  def io: LFSRTopIO
}

class LFSRMMIOBlackBox(val w: Int)
    extends BlackBox(Map("WIDTH" -> IntParam(w))) with HasBlackBoxResource {
  val io = IO(new LFSRIO(w))
  addResource("/vsrc/LFSRMMIOBlackBox.v")
}

class LFSRTL(params: LFSRParams, beatBytes: Int)(implicit p: Parameters)
    extends ClockSinkDomain(ClockSinkParameters())(p) {
  val device = new SimpleDevice("lfsr", Seq("custom,lfsr"))
  val node = TLRegisterNode(
    Seq(AddressSet(params.address, 4096 - 1)),
    device,
    "reg/control",
    beatBytes = beatBytes)

  lazy val module = new LFSRImpl

  class LFSRImpl extends Impl with HasLFSRTopIO {
    val io = IO(new LFSRTopIO)

    val seed = Reg(UInt(params.width.W))
    val stepsIn = Wire(Decoupled(UInt(params.width.W)))
    val resultOut = Wire(Decoupled(UInt(params.width.W)))
    val blackbox = Module(new LFSRMMIOBlackBox(params.width))

    blackbox.io.clock := clock
    blackbox.io.reset := reset.asBool
    blackbox.io.seed := seed
    blackbox.io.steps := stepsIn.bits
    blackbox.io.input_valid := stepsIn.valid
    stepsIn.ready := blackbox.io.input_ready

    resultOut.valid := blackbox.io.output_valid
    resultOut.bits := blackbox.io.lfsr_result
    blackbox.io.output_ready := resultOut.ready

    io.lfsr_busy := blackbox.io.busy

    node.regmap(
      0x00 -> Seq(RegField.r(2, Cat(blackbox.io.input_ready, blackbox.io.output_valid))),
      0x04 -> Seq(RegField.w(params.width, seed)),
      0x08 -> Seq(RegField.w(params.width, stepsIn)),
      0x0c -> Seq(RegField.r(params.width, resultOut)))
  }
}

trait CanHavePeripheryLFSR { this: BaseSubsystem =>
  private val portName = "lfsr"
  private val pbus = locateTLBusWrapper(PBUS)

  val lfsr_busy = p(LFSRKey) match {
    case Some(params) =>
      val lfsr = LazyModule(new LFSRTL(params, pbus.beatBytes)(p))
      lfsr.clockNode := pbus.fixedClockNode
      pbus.coupleTo(portName) {
        TLInwardClockCrossingHelper("lfsr_crossing", lfsr, lfsr.node)(SynchronousCrossing()) :=
          TLFragmenter(pbus.beatBytes, pbus.blockBytes) := _
      }
      Some(InModuleBody {
        val busy = IO(Output(Bool())).suggestName("lfsr_busy")
        busy := lfsr.module.io.lfsr_busy
        busy
      })
    case None => None
  }
}

class WithLFSR extends Config((site, here, up) => {
  case LFSRKey => Some(LFSRParams())
})
