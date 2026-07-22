/*
Testbench do módulo BancoDeRegistradores.
Autores:
    Kayky Moreira Praxedes;
    Carlos Ernesto Cardoso dos Reis.
*/
`timescale 1ns / 1ns
module tb_BancoDeRegistradores;
    // Inputs
    reg [7:0] reEntrada;
    reg Clock, Reset, RegWrite;
    reg [1:0] Reg1, Reg2;
    reg [7:0] dadoEscrever;
    // Outputs
    wire [7:0] dado1, dado2, reSaida;
    // Instância do módulo BancoDeRegistradores
    BancoDeRegistradores uut(.reEntrada(reEntrada), .Clock(Clock), .Reset(Reset), .RegWrite(RegWrite), .Reg1(Reg1), .Reg2(Reg2), .dadoEscrever(dadoEscrever), .dado1(dado1), .dado2(dado2), .reSaida(reSaida));
    always #5 Clock = ~Clock; // Clock com período de 10ns
    initial begin
	// Inicialização
	Clock = 0; Reset = 0; RegWrite = 0; reEntrada = 8'h00; dadoEscrever = 8'h00; Reg1 = 2'b00; Reg2 = 2'b00;
	$display("Time | Clock | Reset | RegWrite | Reg1 | Reg2 | DadoW | Dado Reg1 | Dado Reg2 | ReIn | ReOut");
	$monitor("%3t  |   %b   |   %b   |     %b    |  %b  |  %b  |   %h  |     %h    |     %h    |  %h  |   %h", $time, Clock, Reset, RegWrite, Reg1, Reg2, dadoEscrever, dado1, dado2, reEntrada, reSaida);
	Reset = 1; #10; // Teste de Reset
	Reset = 0; RegWrite = 1; reEntrada = 8'h11; Reg1 = 2'd0; dadoEscrever = 8'hAA; #10; // Teste de escrita em todos registradores (na borda de subida): Reg $c0 = aa
	reEntrada = 8'h22; Reg1 = 2'd1; dadoEscrever = 8'hBB; #10; // Reg $c1 = bb
	reEntrada = 8'h33; Reg1 = 2'd2; dadoEscrever = 8'hCC; #10; // Reg $c2 = cc
	reEntrada = 8'h44; Reg1 = 2'd3; dadoEscrever = 8'hDD; #10; // Reg $c3 = dd
	RegWrite = 0; reEntrada = 8'h55; Reg1 = 2'd0; Reg2 = 2'd1; #10; // Teste de leitura de todos os pares  (Reg1 = $c0, Reg2 = $c1)
	Reg1 = 2'd2; Reg2 = 2'd3; #10; // Reg1 = $c3, Reg2 = $c4
	reEntrada = 8'h66; Reg1 = 2'd0; Reg2 = 2'd0; dadoEscrever = 8'hFF; #10; // Teste escrita sem RegWrite
	Reg1 = 2'd0; #10; // Verifica se não escreveu
	RegWrite = 1; #10; // Verifica que escreveu
	reEntrada = 8'h77; #10; // Teste atualização de reEntrada
	reEntrada = 8'h88; #10;
	Reset = 1; #10; // Reset final
	Reset = 0; #11;
	$finish;
    end
endmodule
