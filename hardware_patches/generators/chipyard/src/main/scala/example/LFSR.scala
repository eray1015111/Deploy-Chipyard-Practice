package chipyard.example

import chisel3._
import chisel3.util._
import chisel3.experimental.{IntParam, BaseModule}
import freechips.rocketchip.prci._
import freechips.rocketchip.subsystem.{BaseSubsystem, PBUS}
import org.chipsalliance.cde.config.{Parameters, Field, Config}
import freechips.rocketchip.diplomacy._
import freechips.rocketchip.regmapper.{HasRegMap, RegField}
import freechips.rocketchip.tilelink._
import freechips.rocketchip.util._

case class LFSRParams(
  address: BigInt = 0x6000,
  width: Int = 32
)

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

class LFSRMMIOBlackBox(val w: Int) extends BlackBox(Map("WIDTH" -> IntParam(w))) with HasBlackBoxResource {
  val io = IO(new LFSRIO(w))
  addResource("/vsrc/LFSRMMIOBlackBox.v")
}

class LFSRTL(params: LFSRParams, beatBytes: Int)(implicit p: Parameters) extends ClockSinkDomain(ClockSinkParameters())(p) {
  val device = new SimpleDevice("lfsr", Seq("custom,lfsr"))
  val node = TLRegisterNode(Seq(AddressSet(params.address, 4096-1)), device, "reg/control", beatBytes=beatBytes)

  override lazy val module = new LFSRImpl
  class LFSRImpl extends Impl with HasLFSRTopIO {
    val io = IO(new LFSRTopIO)
    withClockAndReset(clock, reset) {
      val seed = Reg(UInt(params.width.W))
      val steps_input = Wire(new DecoupledIO(UInt(params.width.W)))
      val lfsr_out = Wire(new DecoupledIO(UInt(params.width.W)))
      val status = Wire(UInt(2.W))

      val impl = Module(new LFSRMMIOBlackBox(params.width))
      val impl_io = impl.io

      impl_io.clock := clock
      impl_io.reset := reset.asBool

      impl_io.seed := seed
      impl_io.steps := steps_input.bits
      impl_io.input_valid := steps_input.valid
      steps_input.ready := impl_io.input_ready

      lfsr_out.bits := impl_io.lfsr_result
      lfsr_out.valid := impl_io.output_valid
      impl_io.output_ready := lfsr_out.ready

      status := Cat(impl_io.input_ready, impl_io.output_valid)
      io.lfsr_busy := impl_io.busy

      node.regmap(
        0x00 -> Seq(RegField.r(2, status)),
        0x04 -> Seq(RegField.w(params.width, seed)),
        0x08 -> Seq(RegField.w(params.width, steps_input)),
        0x0C -> Seq(RegField.r(params.width, lfsr_out))
      )
    }
  }
}

trait CanHavePeripheryLFSR { this: BaseSubsystem =>
  private val portName = "lfsr"
  private val pbus = locateTLBusWrapper(PBUS)

  val lfsr_busy = p(LFSRKey) match {
    case Some(params) => {
      val lfsr = LazyModule(new LFSRTL(params, pbus.beatBytes)(p))
      lfsr.clockNode := pbus.fixedClockNode
      pbus.coupleTo(portName) {
        TLInwardClockCrossingHelper("lfsr_crossing", lfsr, lfsr.node)(SynchronousCrossing()) :=
        TLFragmenter(pbus.beatBytes, pbus.blockBytes) := _
      }
      val lfsr_busy = InModuleBody {
        val busy = IO(Output(Bool())).suggestName("lfsr_busy")
        busy := lfsr.module.io.lfsr_busy
        busy
      }
      Some(lfsr_busy)
    }
    case None => None
  }
}

class WithLFSR extends Config((site, here, up) => {
  case LFSRKey => Some(LFSRParams())
})
