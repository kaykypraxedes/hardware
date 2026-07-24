/*
Testbench do módulo PC.
Autores:
    Kayky Moreira Praxedes;
    Carlos Ernesto Cardoso dos Reis.
*/
`timescale 1ns / 1ns
module tb_Pc;
    // Inputs
    reg [7:0] endereco;
    reg End, Reset, Clock;
    // Outputs
    wire [7:0] enderecoSaida;
    // Instância do módulo Pc
    Pc uut(.endereco(endereco), .End(End), .Reset(Reset), .Clock(Clock), .enderecoSaida(enderecoSaida));
    always #5 Clock = ~Clock; // Geração de um clock com período de 10ns
    initial begin
	Clock = 0;
	$display(" Time | Clock | Reset | End | endereco |enderecoSaida");
	$monitor("  %2t  |   %b   |   %b   |  %b  |   %d    |     %d", $time, Clock, Reset, End, endereco, enderecoSaida);
	Reset = 1; endereco = 8'd5; End = 0; #10; // Reset inicial (enderecoSaida permanece 0)
	Reset = 0; #10;
	endereco = 8'd10; #10; // Atualiza endereço normalmente na borda de subida
	endereco = 8'd20; #10;
	endereco = 8'd30; End = 1; #10; // End = 1 (PC não muda)
	endereco = 8'd40; #10;
	Reset = 1; #10; // Reset no final (zera enderecoSaida)
	Reset = 0; End = 0; #11; // Escreve normalmente o endereço
	$finish; // 11ns para completar o ciclo do clock sem conflitar com o $finish
    end
endmodule
