module MACMMIOBlackBox
  #(parameter WIDTH = 32)
   (
    input                  clock,
    input                  reset,
    output                 input_ready,
    input                  input_valid,
    input [WIDTH-1:0]      a,
    input [WIDTH-1:0]      b,
    input [WIDTH-1:0]      c,
    input                  output_ready,
    output                 output_valid,
    output reg [WIDTH-1:0] mac_result,
    output                 busy
    );

   localparam S_IDLE = 2'b00, S_COMPUTE = 2'b01, S_DONE = 2'b10;

   reg [1:0]               state;
   reg [WIDTH-1:0]         r_a, r_b, r_c;

   assign input_ready = (state == S_IDLE);
   assign output_valid = (state == S_DONE);
   assign busy = (state != S_IDLE);

   always @(posedge clock) begin
      if (reset) begin
         state <= S_IDLE;
         mac_result <= {WIDTH{1'b0}};
         r_a <= {WIDTH{1'b0}};
         r_b <= {WIDTH{1'b0}};
         r_c <= {WIDTH{1'b0}};
      end else begin
         case (state)
            S_IDLE: begin
               if (input_valid) begin
                  r_a <= a;
                  r_b <= b;
                  r_c <= c;
                  state <= S_COMPUTE;
               end
            end
            S_COMPUTE: begin
               mac_result <= (r_a * r_b) + r_c;
               state <= S_DONE;
            end
            S_DONE: begin
               if (output_ready)
                  state <= S_IDLE;
            end
         endcase
      end
   end

endmodule
