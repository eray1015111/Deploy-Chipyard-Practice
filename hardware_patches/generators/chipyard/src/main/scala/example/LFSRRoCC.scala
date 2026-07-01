package chipyard.example

import chisel3._
import chisel3.util._
import freechips.rocketchip.diplomacy._
import freechips.rocketchip.tile._
import org.chipsalliance.cde.config.{Config, Parameters}

class LFSRRoCC(opcodes: OpcodeSet)(implicit p: Parameters) extends LazyRoCC(opcodes) {
  override lazy val module = new LFSRRoCCModuleImp(this)
}

class LFSRRoCCModuleImp(outer: LFSRRoCC)(implicit p: Parameters)
    extends LazyRoCCModuleImp(outer) with HasCoreParameters {
  val cmd = Queue(io.cmd)
  val resp = io.resp

  val sIdle :: sCompute :: sDone :: Nil = Enum(3)
  val state = RegInit(sIdle)
  val seed = Reg(UInt(32.W))
  val steps = Reg(UInt(32.W))
  val rd = Reg(chiselTypeOf(cmd.bits.inst.rd))
  val doResp = RegInit(false.B)

  cmd.ready := state === sIdle

  when(cmd.fire) {
    seed := cmd.bits.rs1(31, 0)
    steps := cmd.bits.rs2(31, 0)
    rd := cmd.bits.inst.rd
    doResp := cmd.bits.inst.xd
    state := Mux(cmd.bits.rs2(31, 0) === 0.U, sDone, sCompute)
  }

  when(state === sCompute) {
    seed := Mux(seed(0), (seed >> 1) ^ 0x80200003L.U(32.W), seed >> 1)
    steps := steps - 1.U
    when(steps === 1.U) {
      state := sDone
    }
  }

  resp.valid := state === sDone && doResp
  resp.bits.rd := rd
  resp.bits.data := seed

  when(state === sDone) {
    when(!doResp || resp.fire) {
      state := sIdle
    }
  }

  io.busy := state =/= sIdle
  io.interrupt := false.B
  io.mem.req.valid := false.B
}

class WithLFSRRoCC extends Config((site, here, up) => {
  case BuildRoCC => up(BuildRoCC, site) ++ Seq((p: Parameters) => {
    LazyModule(new LFSRRoCC(OpcodeSet.custom0)(p))
  })
})
