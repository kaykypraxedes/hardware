/*
Testbench do módulo MemoriaInstrucoes.
Autores:
    Kayky Moreira Praxedes;
    Carlos Ernesto Cardoso dos Reis.
*/
`timescale 1ns / 1ns
module tb_MemoriaInstrucoes;
    // Inputs
    reg [7:0] endereco;
    // Outputs
    wire OpCode2, OpCode1, OpCode0, funct;
    wire [1:0] reg1, reg2;
    wire [2:0] imediato;
    // Instância do módulo MemoriaInstrucoes
    MemoriaInstrucoes uut(.endereco(endereco), .OpCode2(OpCode2), .OpCode1(OpCode1), .OpCode0(OpCode0), .funct(funct), .reg1(reg1),.reg2(reg2), .imediato(imediato));
    initial begin
	// Inicialização
	endereco = 0; #10;
	$display("Endereço | OpCode | Reg1 | Reg2 | Funct | Imediato");
	$monitor("   %2d    |   %b%b%b  |  %b  |  %b  |   %b   |  %b  ", endereco, OpCode2, OpCode1, OpCode0, reg1, reg2, funct, imediato);
	endereco = 0; #10;  // sub $c2, $c2  = 000 10 10 1
	endereco = 4; #10;  // addi $c0, 3   = 001 00 011
	endereco = 10; #10; // ld $c1, $c2   = 011 01 10 0
	endereco = 19; #10; // result $c3, 1 = 110 11 001
	endereco = 54; #10; // addi $c0, 3   = 001 00 011
	endereco = 77; #10; // nop           = 111 0000 0
	endereco = 81; #10; // jr $c0        = 100 00 000
	endereco = 83; #10; // hlt           = 111 0000 1
	$finish;
    end
endmodule
