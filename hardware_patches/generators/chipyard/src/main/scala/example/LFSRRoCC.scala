package chipyard.example

import chisel3._
import chisel3.util._
import freechips.rocketchip.tile._
import freechips.rocketchip.diplomacy._
import org.chipsalliance.cde.config.{Parameters, Field, Config}

class LFSRRoCC(opcodes: OpcodeSet)(implicit p: Parameters) extends LazyRoCC(opcodes) {
  override lazy val module = new LFSRRoCCModuleImp(this)
}

class LFSRRoCCModuleImp(outer: LFSRRoCC)(implicit p: Parameters) extends LazyRoCCModuleImp(outer)
    with HasCoreParameters {
  val cmd = Queue(io.cmd)
  val resp = io.resp

  val s_idle :: s_compute :: s_done :: Nil = Enum(3)
  val state = RegInit(s_idle)

  val seed = Reg(UInt(32.W))
  val steps = Reg(UInt(32.W))
  val rd = Reg(chiselTypeOf(cmd.bits.inst.rd))

  // Receive command
  cmd.ready := (state === s_idle)

  when (cmd.fire) {
    seed := cmd.bits.rs1(31, 0)
    steps := cmd.bits.rs2(31, 0)
    rd := cmd.bits.inst.rd
    state := Mux(cmd.bits.rs2(31, 0) === 0.U, s_done, s_compute)
  }

  // Compute LFSR
  when (state === s_compute) {
    seed := Mux(seed(0), (seed >> 1) ^ 0x80200003L.U, seed >> 1)
    steps := steps - 1.U
    when (steps === 1.U) {
      state := s_done
    }
  }

  // Send response
  val doResp = RegEnable(cmd.bits.inst.xd, false.B, cmd.fire) // Does the instruction expect a response?
  
  resp.valid := (state === s_done) && doResp
  resp.bits.rd := rd
  resp.bits.data := seed // extended to xLen

  when (state === s_done) {
    when (!doResp || resp.fire) {
      state := s_idle
    }
  }

  io.busy := (state =/= s_idle)
  io.interrupt := false.B
  io.mem.req.valid := false.B
}

class WithLFSRRoCC extends Config((site, here, up) => {
  case BuildRoCC => up(BuildRoCC) ++ Seq(
    (p: Parameters) => {
      val lfsr = LazyModule(new LFSRRoCC(OpcodeSet.custom0)(p))
      lfsr
    }
  )
})
