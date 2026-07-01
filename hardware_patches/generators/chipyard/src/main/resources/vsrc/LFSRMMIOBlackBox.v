module LFSRMMIOBlackBox
  #(parameter WIDTH = 32)
   (
    input                  clock,
    input                  reset,
    output                 input_ready,
    input                  input_valid,
    input [WIDTH-1:0]      seed,
    input [WIDTH-1:0]      steps,
    input                  output_ready,
    output                 output_valid,
    output reg [WIDTH-1:0] lfsr_result,
    output                 busy
    );

   localparam S_IDLE = 2'b00, S_COMPUTE = 2'b01, S_DONE = 2'b10;

   reg [1:0]               state;
   reg [WIDTH-1:0]         r_steps;

   assign input_ready = (state == S_IDLE);
   assign output_valid = (state == S_DONE);
   assign busy = (state != S_IDLE);

   always @(posedge clock) begin
      if (reset) begin
         state <= S_IDLE;
         lfsr_result <= {WIDTH{1'b0}};
         r_steps <= {WIDTH{1'b0}};
      end else begin
         case (state)
            S_IDLE: begin
               if (input_valid) begin
                  lfsr_result <= seed;
                  r_steps <= steps;
                  if (steps == 0)
                     state <= S_DONE;
                  else
                     state <= S_COMPUTE;
               end
            end
            S_COMPUTE: begin
               if (lfsr_result[0]) begin
                   lfsr_result <= (lfsr_result >> 1) ^ 32'h80200003;
               end else begin
                   lfsr_result <= (lfsr_result >> 1);
               end
               
               r_steps <= r_steps - 1;
               if (r_steps == 1) begin
                   state <= S_DONE;
               end
            end
            S_DONE: begin
               if (output_ready)
                  state <= S_IDLE;
            end
         endcase
      end
   end

endmodule
