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

case class MACParams(
  address: BigInt = 0x5000,
  width: Int = 32,
  useBlackBox: Boolean = true
)

case object MACKey extends Field[Option[MACParams]](None)

class MACIO(val w: Int) extends Bundle {
  val clock = Input(Clock())
  val reset = Input(Bool())
  val input_ready = Output(Bool())
  val input_valid = Input(Bool())
  val a = Input(UInt(w.W))
  val b = Input(UInt(w.W))
  val c = Input(UInt(w.W))
  val output_ready = Input(Bool())
  val output_valid = Output(Bool())
  val mac_result = Output(UInt(w.W))
  val busy = Output(Bool())
}

class MACTopIO extends Bundle {
  val mac_busy = Output(Bool())
}

trait HasMACTopIO {
  def io: MACTopIO
}

class MACMMIOBlackBox(val w: Int) extends BlackBox(Map("WIDTH" -> IntParam(w))) with HasBlackBoxResource {
  val io = IO(new MACIO(w))
  addResource("/vsrc/MACMMIOBlackBox.v")
}

class MACMMIOChiselModule(val w: Int) extends Module {
  val io = IO(new MACIO(w))
  val s_idle :: s_compute :: s_done :: Nil = Enum(3)

  val state = RegInit(s_idle)
  val r_a   = Reg(UInt(w.W))
  val r_b   = Reg(UInt(w.W))
  val r_c   = Reg(UInt(w.W))
  val result = Reg(UInt(w.W))

  io.input_ready := state === s_idle
  io.output_valid := state === s_done
  io.mac_result := result
  io.busy := state =/= s_idle

  when (state === s_idle && io.input_valid) {
    r_a := io.a
    r_b := io.b
    r_c := io.c
    state := s_compute
  } .elsewhen (state === s_compute) {
    result := (r_a * r_b) + r_c
    state := s_done
  } .elsewhen (state === s_done && io.output_ready) {
    state := s_idle
  }
}

class MACTL(params: MACParams, beatBytes: Int)(implicit p: Parameters) extends ClockSinkDomain(ClockSinkParameters())(p) {
  val device = new SimpleDevice("mac", Seq("custom,mac"))
  val node = TLRegisterNode(Seq(AddressSet(params.address, 4096-1)), device, "reg/control", beatBytes=beatBytes)

  override lazy val module = new MACImpl
  class MACImpl extends Impl with HasMACTopIO {
    val io = IO(new MACTopIO)
    withClockAndReset(clock, reset) {
      val a = Reg(UInt(params.width.W))
      val b = Reg(UInt(params.width.W))
      val c_input = Wire(new DecoupledIO(UInt(params.width.W)))
      val mac_out = Wire(new DecoupledIO(UInt(params.width.W)))
      val status = Wire(UInt(2.W))

      val impl_io = if (params.useBlackBox) {
        val impl = Module(new MACMMIOBlackBox(params.width))
        impl.io
      } else {
        val impl = Module(new MACMMIOChiselModule(params.width))
        impl.io
      }

      impl_io.clock := clock
      impl_io.reset := reset.asBool

      impl_io.a := a
      impl_io.b := b
      impl_io.c := c_input.bits
      impl_io.input_valid := c_input.valid
      c_input.ready := impl_io.input_ready

      mac_out.bits := impl_io.mac_result
      mac_out.valid := impl_io.output_valid
      impl_io.output_ready := mac_out.ready

      status := Cat(impl_io.input_ready, impl_io.output_valid)
      io.mac_busy := impl_io.busy

      node.regmap(
        0x00 -> Seq(
          RegField.r(2, status)),
        0x04 -> Seq(
          RegField.w(params.width, a)),
        0x08 -> Seq(
          RegField.w(params.width, b)),
        0x0C -> Seq(
          RegField.w(params.width, c_input)),
        0x10 -> Seq(
          RegField.r(params.width, mac_out))
      )
    }
  }
}

trait CanHavePeripheryMAC { this: BaseSubsystem =>
  private val portName = "mac"
  private val pbus = locateTLBusWrapper(PBUS)

  val mac_busy = p(MACKey) match {
    case Some(params) => {
      val mac = LazyModule(new MACTL(params, pbus.beatBytes)(p))
      mac.clockNode := pbus.fixedClockNode
      pbus.coupleTo(portName) {
        TLInwardClockCrossingHelper("mac_crossing", mac, mac.node)(SynchronousCrossing()) :=
        TLFragmenter(pbus.beatBytes, pbus.blockBytes) := _
      }
      val mac_busy = InModuleBody {
        val busy = IO(Output(Bool())).suggestName("mac_busy")
        busy := mac.module.io.mac_busy
        busy
      }
      Some(mac_busy)
    }
    case None => None
  }
}

class WithMAC(useBlackBox: Boolean = true) extends Config((site, here, up) => {
  case MACKey => Some(MACParams(useBlackBox = useBlackBox))
})
