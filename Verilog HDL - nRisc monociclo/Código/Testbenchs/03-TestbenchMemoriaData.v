/*
Testbench do módulo MemoriaData.
Autores:
    Kayky Moreira Praxedes;
    Carlos Ernesto Cardoso dos Reis.
*/
`timescale 1ns / 1ns
module tb_MemoriaData;
    // Inputs
    reg [7:0] endereco;
    reg [7:0] dadoEscrever;
    reg MemRead, MemWrite, Clock;
    // Outputs
    wire [7:0] dadoLido;
    // Instância do módulo MemoriaData
    MemoriaData uut(.endereco(endereco), .dadoEscrever(dadoEscrever), .MemRead(MemRead), .MemWrite(MemWrite), .Clock(Clock), .dadoLido(dadoLido));
    always #5 Clock = ~Clock; // Clock com período de 10ns 
    initial begin
        // Inicialização
	Clock = 0; endereco = 0; dadoEscrever = 0; MemRead = 0; MemWrite = 0;
	$display("Endereço | Dado lido"); MemRead = 1; // Leitura da uma mensagem
	for (integer i = 0; i < 6; i = i + 1) begin
	    endereco = i; #10;
	    $display("   %2d    | %8b (%s)", i, dadoLido, dadoLido);
	end
	MemRead = 0; MemWrite = 1; #10; // Armazenamento de nova mensagem
	endereco = 6; dadoEscrever = 8'b01010111; #10; // 'W'
	endereco = 7; dadoEscrever = 8'b01001111; #10; // 'O'
	endereco = 8; dadoEscrever = 8'b01010010; #10; // 'R'
	endereco = 9; dadoEscrever = 8'b01001100; #10; // 'L'
	endereco = 10; dadoEscrever = 8'b01000100; #10; // 'D'
	MemWrite = 0; MemRead = 1; #10; // Leitura da mensagem composta
	for (integer i = 0; i < 11; i = i + 1) begin 
	    endereco = i; #10;
	    $display("   %2d    | %8b (%s)", i, dadoLido, dadoLido);
	end
	MemRead = 0; MemWrite = 1; #10; // Sobreescrita de dados
	endereco = 0; dadoEscrever = 8'b01010111; #10; // 'W'
	endereco = 1; dadoEscrever = 8'b01001111; #10; // 'O'
	endereco = 2; dadoEscrever = 8'b01010010; #10; // 'R'
	endereco = 3; dadoEscrever = 8'b01001100; #10; // 'L'
	endereco = 4; dadoEscrever = 8'b01000100; #10; // 'D'
	MemWrite = 0; MemRead = 1; #10; // Leitura da mensagem sobreescrita
	for (integer i = 0; i < 11; i = i + 1) begin
	    endereco = i; #10;
	    $display("   %2d    | %8b (%s)", i, dadoLido, dadoLido);
	end
	endereco = 20; #10; // Tenta ler endereço de memória não inicializado
	$display("   %2d    | %8b (%s)", endereco, dadoLido, dadoLido); 
	MemWrite = 1; #10; // Leitura e escrita ao mesmo tempo
	dadoEscrever = 8'b01011010; #10; // 'Z'
	$display("   %2d    | %8b (%s)", endereco, dadoLido, dadoLido);
	MemWrite = 0; #10; // Escrita sem o MemWrite
	dadoEscrever = 8'b01000100; #10; // 'D'
	$display("   %2d    | %8b (%s)", endereco, dadoLido, dadoLido); $finish;
    end
endmodule
