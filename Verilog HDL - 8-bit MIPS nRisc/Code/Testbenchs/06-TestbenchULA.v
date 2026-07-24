/*
Testbench do módulo ULA.
Autores:
    Kayky Moreira Praxedes;
    Carlos Ernesto Cardoso dos Reis.
*/
`timescale 1ns / 1ns
module tb_ULA;
    // Inputs
    reg [7:0] A;
    reg [7:0] B;
    reg [7:0] reEntrada;
    reg ALUOp1, ALUOp0, funct, RegWrite;
    // Outputs
    wire [7:0] S;
    wire [7:0] reSaida;
    wire zero, overflow;
    // Instância do módulo Ula
    Ula uut(.A(A), .B(B), .reEntrada(reEntrada), .ALUOp1(ALUOp1), .ALUOp0(ALUOp0), .funct(funct), .RegWrite(RegWrite), .S(S), .reSaida(reSaida), .zero(zero), .overflow(overflow));
    initial begin
	// Initialize Inputs
	$display("  A  |  B  |  S  | overflow | reEntrada | reSaida | zero");
	$monitor(" %d | %d | %d |     %b    |     %h    |    %h   |  %b", A, B, S, overflow, reEntrada, reSaida, zero);
	ALUOp1 = 0; ALUOp0 = 0; funct = 0; RegWrite = 1; A = 8'd10; B = 8'd20; reEntrada = 8'hAA; #10; // ADD (normal)
	ALUOp1 = 0; ALUOp0 = 0; funct = 0; RegWrite = 1; A = 8'd255; B = 8'd1; reEntrada = 8'h55; #10; // ADD (overflow)
	ALUOp1 = 0; ALUOp0 = 0; funct = 1; RegWrite = 1; A = 8'd30; B = 8'd15; reEntrada = 8'hBB; #10; // SUB (normal)
	ALUOp1 = 0; ALUOp0 = 1; RegWrite = 1; A = 8'd50; B = 8'd25; reEntrada = 8'hEE; #10; // ADDI
	ALUOp1 = 1; ALUOp0 = 0; RegWrite = 1; A = 8'd10; B = 8'd5; reEntrada = 8'hFF; #10; // MULT (normal)
	A = 8'd128; B = 8'd2; reEntrada = 8'h66; #10; // MULT (overflow)
	ALUOp1 = 1; ALUOp0 = 1; funct = 0; RegWrite = 0; A = 8'd40; B = 8'd40; reEntrada = 8'h11; #10; // BEQ (valores iguais)
	B = 8'd41; reEntrada = 8'h22; #10; // BEQ (valores diferentes)
	ALUOp1 = 1; ALUOp0 = 1; funct = 1; RegWrite = 0; A = 8'd10; B = 8'd20; reEntrada = 8'h33; #10; // SLT (A < B)
	A = 8'd30; B = 8'd20; reEntrada = 8'h44; #10;  // SLT (A >= B)
	A = 8'd25; B = 8'hCC; reEntrada = 8'hCC; #10; // RESULT (valores iguais)
	ALUOp1 = 0; ALUOp0 = 0; RegWrite = 0; A = 8'd30; B = 8'd25; reEntrada = 8'hDD; #10; // RESULT (valores diferentes)
	$finish;
    end
endmodule
