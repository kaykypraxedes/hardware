/*
Testbench do módulo Controle.
Autores:
    Kayky Moreira Praxedes;
    Carlos Ernesto Cardoso dos Reis.
*/
`timescale 1ns / 1ns // Escala de tempo (Unidade = 1ns, Precisão = 1ns)
module tb_Controle;
    // Inputs
    reg Op2, Op1, Op0, funct, reset;
    // Outputs
    wire ALUSrc, MemToReg, RegWrite, MemRead, MemWrite, Branch, Jump, NextOp, End, ALUOp1, ALUOp0;
    // Instância do módulo a ser testado (Controle)
    Controle uut(.Op2(Op2), .Op1(Op1), .Op0(Op0), .funct(funct), .reset(reset), .ALUSrc(ALUSrc), .MemToReg(MemToReg), .RegWrite(RegWrite), .MemRead(MemRead), .MemWrite(MemWrite), .Branch(Branch), .Jump(Jump), .NextOp(NextOp), .End(End), .ALUOp1(ALUOp1), .ALUOp0(ALUOp0));
    initial begin
	$display(" Op2 Op1 Op0 | funct | reset | Sinais de controle");
	$monitor("  %b   %b   %b  |   %b   |   %b   | %b | %b | %b | %b | %b | %b | %b | %b | %b | %b | %b ", Op2, Op1, Op0, funct, reset, ALUSrc, MemToReg, RegWrite, MemRead, MemWrite, Branch, Jump, NextOp, End, ALUOp1, ALUOp0);
	reset = 1; {Op2, Op1, Op0} = 3'b000; funct = 0; #10; // Reset ativo
	reset = 0; // Reset inativo
	{Op2, Op1, Op0} = 3'b000; funct = 0; #10; // ADD
	{Op2, Op1, Op0} = 3'b000; funct = 1; #10; // SUB
	{Op2, Op1, Op0} = 3'b001; funct = 0; #10; // ADDI
	{Op2, Op1, Op0} = 3'b001; funct = 1; #10; // ADDI
	{Op2, Op1, Op0} = 3'b010; funct = 0; #10; // MULT
	{Op2, Op1, Op0} = 3'b010; funct = 1; #10; // ~null
	{Op2, Op1, Op0} = 3'b011; funct = 0; #10; // ST
	{Op2, Op1, Op0} = 3'b011; funct = 1; #10; // LD
	{Op2, Op1, Op0} = 3'b100; funct = 0; #10; // JR
	{Op2, Op1, Op0} = 3'b100; funct = 1; #10; // ~null
	{Op2, Op1, Op0} = 3'b101; funct = 0; #10; // BEQ
	{Op2, Op1, Op0} = 3'b101; funct = 1; #10; // SLT
	{Op2, Op1, Op0} = 3'b110; funct = 0; #10; // RESULT
	{Op2, Op1, Op0} = 3'b110; funct = 1; #10; // ~null
	{Op2, Op1, Op0} = 3'b111; funct = 0; #10; // NOP
	{Op2, Op1, Op0} = 3'b111; funct = 1; #10; // HLT
	reset = 1; #10; // Reset 0 e 1 com uma instrução (HLT)
	reset = 0; #10; $finish;
    end
endmodule
