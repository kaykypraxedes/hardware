/*
Testbench do nRisc.
Autores:
    Kayky Moreira Praxedes;
    Carlos Ernesto Cardoso dos Reis.
*/
`timescale 1ns / 1ns
module tb_nRisc;
    // Inputs
    reg Clock, Reset;
    // Outputs
    wire [7:0] PC;
    wire [7:0] Instruction;
    wire [7:0] Reg0, Reg1, Reg2, Reg3, RegRE;
    // Instância do Processador
    nRisc uub(.Clock(Clock), .Reset(Reset));
    // Sinais internos do processador
    assign PC = uub.enderecoPC;
    assign Instruction = uub.memInst_.memoria[uub.enderecoPC];
    assign Reg0 = uub.bancoReg_.registradores[0];
    assign Reg1 = uub.bancoReg_.registradores[1];
    assign Reg2 = uub.bancoReg_.registradores[2];
    assign Reg3 = uub.bancoReg_.registradores[3];
    assign RegRE = uub.bancoReg_.reSaida;
    always #5 Clock = ~Clock;
    // Tarefa para decodificar instruções
    function [8*20:1] decode_instruction;
    	input [7:0] instr;
    	reg [8*20:1] temp;
    	begin
	    casez(instr[7:5])
		3'b000: begin // sub ou add
		    if(instr[0] == 0) temp = "add";
		    else temp = "sub";
		    decode_instruction = {temp, " $c", 8'(instr[4:3]+48), ", $c", 8'(instr[2:1]+48)};
		end        
		3'b001: begin // addi
		    decode_instruction = {"addi $c", 8'(instr[4:3]+48), ", ", 8'(instr[2:0]+48)};
		end
		3'b010: begin // mult
		    decode_instruction = {"mult $c", 8'(instr[4:3]+48), ", $c", 8'(instr[2:1]+48)};
		end        
		3'b011: begin // ld ou st
		    if(instr[0] == 0) temp = "ld";
		    else temp = "st";
		    decode_instruction = {temp, " $c", 8'(instr[4:3]+48), ", $c", 8'(instr[2:1]+48)};
		end        
		3'b100: begin // jr
		    decode_instruction = {"jr $c", 8'(instr[4:3]+48)};
		end
		3'b101: begin // beq ou slt
		    if(instr[0] == 0) temp = "beq";
		    else temp = "slt";
		    decode_instruction = {temp, " $c", 8'(instr[4:3]+48), ", $c", 8'(instr[2:1]+48)};
		end        
	        3'b110: begin // result
		    decode_instruction = {"result $c", 8'(instr[4:3]+48), ", ", 8'(instr[0]+48)};
		end        
		3'b111: begin // nop ou hlt
		    if(instr[0] == 0) decode_instruction = "nop";
		    else decode_instruction = "hlt";
		end
            endcase
	end
    endfunction
    // Tarefa para mostrar estado da memória de dados
    task show_data_memory;
	input [7:0] addr_start;
	input [7:0] addr_end;
	integer i;
	begin
	    $display("Memoria de dados [%0d:%0d]:", addr_start, addr_end);
	    $display(" Endereco |    Valor   | Decimal | Caractere");
	    for(i = addr_start; i <= addr_end; i = i + 1) begin
		$display("    %2d    |  %8b  |   %3d   |     %s", i, uub.memData_.memoria[i], uub.memData_.memoria[i], uub.memData_.memoria[i]);
	    end
        end
    endtask
    initial begin // Execução do teste
        show_data_memory(0, 6); // Mostra conteúdo inicial dos endereços de memória [0-6] (Aumentar para mensagens maiores)
	Clock = 0; Reset = 1; #10; // Reset inicial
	Reset = 0; #5; // Aguarda estabilização
	$display(" PC | Instrução |     Assembly    |  \$c0  |  \$c1  |  \$c2  |  \$c3  |  \$re");
	// Monitorar execução
	forever begin
	    @(posedge Clock); // Operação toda borda de subida
	    $display("%3d |  %8b | %-16s| %3d   | %3d   | %3d   | %3d   | %3d", PC, Instruction, decode_instruction(Instruction), Reg0, Reg1, Reg2, Reg3, $signed(RegRE)); // Caso queira resumir apenas para o resultado inicial e final, retirar essa parte
	    if (Instruction == 8'b11100001) begin // Para quando encontra a instrução HLT
	        show_data_memory(0, 6);
	        $finish; // Mostra conteúdo final (endereços de memória [0-6])
	    end
	end
    end
endmodule
