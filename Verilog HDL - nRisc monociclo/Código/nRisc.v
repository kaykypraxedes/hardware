/*
Código em VerilogHDL de um processador nRisc monociclo de 8 bits.
Autores:
    Kayky Moreira Praxedes;
    Carlos Ernesto Cardoso dos Reis.
*/
/*
somador de 1 bit
*/
module somador1bit(
    input A, B, sinal, CarryIn, 
    output S, CarryOut
    );
    wire Bsigned = B ^ sinal;
    assign S = A ^ Bsigned ^ CarryIn;
    assign CarryOut = (A & Bsigned) | (A & CarryIn) | (Bsigned & CarryIn);
endmodule
/*
somador de 8 bits
*/
module somador8bits(
    input [7:0] A, B, 
    input sinal, CarryIn, 
    output [7:0] S, 
    output CarryOut
    );
    wire [6:0]aux;
    somador1bit s8b0(.A(A[0]), .B(B[0]), .sinal(sinal), .CarryIn(CarryIn), .S(S[0]), .CarryOut(aux[0]));
    somador1bit s8b1(.A(A[1]), .B(B[1]), .sinal(sinal), .CarryIn(aux[0]), .S(S[1]), .CarryOut(aux[1]));
    somador1bit s8b2(.A(A[2]), .B(B[2]), .sinal(sinal), .CarryIn(aux[1]), .S(S[2]), .CarryOut(aux[2]));
    somador1bit s8b3(.A(A[3]), .B(B[3]), .sinal(sinal), .CarryIn(aux[2]), .S(S[3]), .CarryOut(aux[3]));
    somador1bit s8b4(.A(A[4]), .B(B[4]), .sinal(sinal), .CarryIn(aux[3]), .S(S[4]), .CarryOut(aux[4]));
    somador1bit s8b5(.A(A[5]), .B(B[5]), .sinal(sinal), .CarryIn(aux[4]), .S(S[5]), .CarryOut(aux[5]));
    somador1bit s8b6(.A(A[6]), .B(B[6]), .sinal(sinal), .CarryIn(aux[5]), .S(S[6]), .CarryOut(aux[6]));
    somador1bit s8b7(.A(A[7]), .B(B[7]), .sinal(sinal), .CarryIn(aux[6]), .S(S[7]), .CarryOut(CarryOut));
endmodule
/*
somador de 16 bits
*/
module somador16bits(
    input [15:0] A, B, 
    input sinal, CarryIn,
    output [15:0] S, 
    output CarryOut
    );
    wire aux;
    somador8bits s16b0(.A(A[7:0]), .B(B[7:0]), .sinal(sinal), .CarryIn(CarryIn), .S(S[7:0]), .CarryOut(aux));
    somador8bits s16b1(.A(A[15:8]), .B(B[15:8]), .sinal(sinal), .CarryIn(aux), .S(S[15:8]), .CarryOut(CarryOut));
endmodule
/*
multiplicador de 8 bits
*/
module multiplicador8bits(
    input [7:0] A, B, 
    output [7:0] S, 
    output overflow
    );
    wire sinal = A[7] ^ B[7];
    // módulo de A e B
    wire [7:0] modA, modB;
    wire auxA, auxB;
    somador8bits s8bA(.A(8'b0), .B(A), .sinal(A[7]), .CarryIn(A[7]), .S(modA), .CarryOut(auxA));
    somador8bits s8bB(.A(8'b0), .B(B), .sinal(B[7]), .CarryIn(B[7]), .S(modB), .CarryOut(auxB));
    // multiplicação
    wire [15:0] r0, r1, r2, r3, r4, r5, r6, r7;
    wire aux0, aux1, aux2, aux3, aux4, aux5, aux6, aux7; 
    somador16bits s16b0(.A({8'b0, (modA & {8{modB[0]}})}), .B(16'b0), .sinal(1'b0), .CarryIn(1'b0), .S(r0), .CarryOut(aux0));
    somador16bits s16b1(.A({7'b0, (modA & {8{modB[1]}}), 1'b0}), .B(r0), .sinal(1'b0), .CarryIn(1'b0), .S(r1), .CarryOut(aux1));
    somador16bits s16b2(.A({6'b0, (modA & {8{modB[2]}}), 2'b0}), .B(r1), .sinal(1'b0), .CarryIn(1'b0), .S(r2), .CarryOut(aux2));
    somador16bits s16b3(.A({5'b0, (modA & {8{modB[3]}}), 3'b0}), .B(r2), .sinal(1'b0), .CarryIn(1'b0), .S(r3), .CarryOut(aux3));
    somador16bits s16b4(.A({4'b0, (modA & {8{modB[4]}}), 4'b0}), .B(r3), .sinal(1'b0), .CarryIn(1'b0), .S(r4), .CarryOut(aux4));
    somador16bits s16b5(.A({3'b0, (modA & {8{modB[5]}}), 5'b0}), .B(r4), .sinal(1'b0), .CarryIn(1'b0), .S(r5), .CarryOut(aux5));
    somador16bits s16b6(.A({2'b0, (modA & {8{modB[6]}}), 6'b0}), .B(r5), .sinal(1'b0), .CarryIn(1'b0), .S(r6), .CarryOut(aux6));
    somador16bits s16b7(.A({1'b0, (modA & {8{modB[7]}}), 7'b0}), .B(r6), .sinal(1'b0), .CarryIn(1'b0), .S(r7), .CarryOut(aux7));
    //sinal
    wire [7:0] signedS = r7[7:0];
    wire auxS;
    somador8bits s8bS(.A(8'b0), .B(signedS), .sinal(sinal), .CarryIn(sinal), .S(S), .CarryOut(auxS));
    assign overflow = |r7[15:8];
endmodule
/*
Controle
*/
module Controle (
    input Op2, Op1, Op0, funct, reset,
    output ALUSrc, MemToReg, RegWrite, MemRead, MemWrite, Branch, Jump, NextOp, End, ALUOp1, ALUOp0
    );
    assign ALUSrc = ((~Op2 & ~Op1 & Op0) | (Op2 & Op1)) & (~reset);
    assign MemToReg = (Op1 & Op0 & ~funct) & (~reset);
    assign RegWrite = ((~Op2 & ~funct) | (~Op2 & ~Op1)) & (~reset);
    assign MemRead = (~Op2 & Op1 & Op0 & ~funct) & (~reset);
    assign MemWrite = (~Op2 & Op1 & funct) & (~reset);
    assign Branch = (Op2 & Op1 & ~Op0) & (~reset);
    assign Jump = (Op2 & ~Op1 & ~Op0) & (~reset);
    assign NextOp = (Op2 & Op1 & Op0 & ~funct) & (~reset);
    assign End = (Op2 & Op1 & Op0 & funct) & (~reset);
    assign ALUOp1 = ((Op2 & ~Op1 & Op0) | (~Op2 & Op1 & ~Op0)) & (~reset);
    assign ALUOp0 = (Op0 | (Op2 & ~Op1)) & (~reset);
endmodule
/*
PC
*/
module Pc(
    input [7:0] endereco,
    input End, Reset, Clock,
    output reg [7:0] enderecoSaida
    );
    always @(posedge Clock or posedge Reset) begin
        if (Reset) enderecoSaida <= 8'b0; // Zera PC no reset
        else if (!End) enderecoSaida <= endereco; // Atualiza apenas se End = 0
    end // Se End = 1, mantém o valor atual (pausa a execução)
endmodule
/*
Memoria de Dados
*/
module MemoriaData(
    input [7:0] endereco,
    input [7:0] dadoEscrever,
    input MemRead, MemWrite, Clock,
    output reg [7:0] dadoLido 
    );
    // Memória de 128 posições com 8 bits
    reg [7:0] memoria [0:127];
    initial begin
        memoria[0] = 8'b01001000;  // 'H'
        memoria[1] = 8'b01000101;  // 'E'
        memoria[2] = 8'b01001100;  // 'L'
        memoria[3] = 8'b01001100;  // 'L'
        memoria[4] = 8'b01001111;  // 'O'
        memoria[5] = 8'b00000000;  // EOF
    end
    // Leitura
    always @(*) begin
        if (MemRead) dadoLido = memoria[endereco];
    end
    // Escrita
    always @(posedge Clock) begin
        if (MemWrite) memoria[endereco] <= dadoEscrever;
    end
endmodule
/*
Memoria de Instrucoes
*/
module MemoriaInstrucoes(
    input [7:0] endereco,
    output reg OpCode2, OpCode1, OpCode0, funct,
    output reg [1:0] reg1, reg2,	
    output reg [2:0] imediato
    );
    reg [7:0] memoria [0:127]; // Memória de 128 posições com 8 bits
    initial begin
	memoria[0] = 8'b00010101;   // sub $c2, $c2
	memoria[1] = 8'b00000001;   // sub $c0, $c0
	memoria[2] = 8'b00011111;   // sub $c3, $c3
	memoria[3] = 8'b11100000;   // nop
	memoria[4] = 8'b00100011;   // addi $c0, 3
	memoria[5] = 8'b00100010;   // addi $c0, 2
	memoria[6] = 8'b00100000;   // addi $c0, 0
	memoria[7] = 8'b00100000;   // addi $c0, 0
	memoria[8] = 8'b00100000;   // addi $c0, 0
	memoria[9] = 8'b11100000;   // nop
	memoria[10] = 8'b01101100;  // ld $c1, $c2
	memoria[11] = 8'b00001000;  // add $c1, $c0
	memoria[12] = 8'b10101000;  // beq $c1, $c0
	memoria[13] = 8'b00000001;  // sub $c0, $c0
	memoria[14] = 8'b11100000;  // nop
	memoria[15] = 8'b00111011;  // addi $c3, 3
	memoria[16] = 8'b01011110;  // mult $c3, $c3
	memoria[17] = 8'b01011110;  // mult $c3, $c3
	memoria[18] = 8'b00111001;  // addi $c3, 1
	memoria[19] = 8'b11011001;  // result $c3, 1
	memoria[20] = 8'b00000001;  // sub $c0, $c0
	memoria[21] = 8'b00011111;  // sub $c3, $c3
	memoria[22] = 8'b11100000;  // nop
	memoria[23] = 8'b00100011;  // addi $c0, 3
	memoria[24] = 8'b00100011;  // addi $c0, 3
	memoria[25] = 8'b00100011;  // addi $c0, 3
	memoria[26] = 8'b00100001;  // addi $c0, 1
	memoria[27] = 8'b00111011;  // addi $c3, 3
	memoria[28] = 8'b00111011;  // addi $c3, 3
	memoria[29] = 8'b00111011;  // addi $c3, 3
	memoria[30] = 8'b01000110;  // mult $c0, $c3
	memoria[31] = 8'b00100001;  // addi $c0, 1
	memoria[32] = 8'b00011111;  // sub $c3, $c3
	memoria[33] = 8'b10101001;  // slt $c1, $c0
	memoria[34] = 8'b00000001;  // sub $c0, $c0
	memoria[35] = 8'b00011111;  // sub $c3, $c3
	memoria[36] = 8'b00111011;  // addi $c3, 3
	memoria[37] = 8'b00000110;  // add $c0, $c3
	memoria[38] = 8'b01011000;  // mult $c3, $c0
	memoria[39] = 8'b01011000;  // mult $c3, $c0
	memoria[40] = 8'b00011110;  // add $c3, $c3
	memoria[41] = 8'b00111101;  // $addi $c3, -3
	memoria[42] = 8'b11011001;  // result $c3, 1
	memoria[43] = 8'b00011111;  // sub $c3, $c3
	memoria[44] = 8'b00000001;  // sub $c0, $c0
	memoria[45] = 8'b00100011;  // addi $c0, 3
	memoria[46] = 8'b00111011;  // addi $c3, 3
	memoria[47] = 8'b01000110;  // mult $c0, $c3
	memoria[48] = 8'b01000110;  // mult $c0, $c3
	memoria[49] = 8'b00100111;  // addi $c0, -1
	memoria[50] = 8'b00001001;  // sub $c1, $c0
	memoria[51] = 8'b11100000;  // nop
	memoria[52] = 8'b00000001;  // sub $c0, $c0
	memoria[53] = 8'b00011111;  // sub $c3, $c3
	memoria[54] = 8'b00100011;  // addi $c0, 3
	memoria[55] = 8'b00100001;  // addi $c0, 1
	memoria[56] = 8'b00011000;  // add $c3, $c0
	memoria[57] = 8'b01000110;  // mult $c0, $c3
	memoria[58] = 8'b01000110;  // mult $c0, $c3
	memoria[59] = 8'b00100001;  // addi $c0, 1
	memoria[60] = 8'b00011111;  // sub $c3, $c3
	memoria[61] = 8'b10101001;  // slt $c1, $c0
	memoria[62] = 8'b00000001;  // sub $c0, $c0
	memoria[63] = 8'b00111011;  // addi $c3, 3
	memoria[64] = 8'b01011110;  // mult $c3, $c3
	memoria[65] = 8'b01011110;  // mult $c3, $c3
	memoria[66] = 8'b00111101;  // addi $c3, -3
	memoria[67] = 8'b11011000;  // result $c3, 0
	memoria[68] = 8'b00011111;  // sub $c3, $c3
	memoria[69] = 8'b00000001;  // sub $c0, $c0
	memoria[70] = 8'b00100011;  // addi $c0, 3
	memoria[71] = 8'b00111011;  // addi $c3, 3
	memoria[72] = 8'b01000110;  // mult $c0, $c3
	memoria[73] = 8'b01000110;  // mult $c0, $c3
	memoria[74] = 8'b00100111;  // addi $c0, -1
	memoria[75] = 8'b00001000;  // add $c1, $c0
	memoria[76] = 8'b00000001;  // sub $c0, $c0
	memoria[77] = 8'b11100000;  // nop
	memoria[78] = 8'b01101101;  // st $c1, $c2
	memoria[79] = 8'b00110001;  // addi $c2, 1
	memoria[80] = 8'b00100001;  // addi $c0, 1
	memoria[81] = 8'b10000000;  // jr $c0
	memoria[82] = 8'b11100000;  // nop
	memoria[83] = 8'b11100001;  // hlt
    end
    always @(endereco) begin // Interpreta as instruções
        OpCode2 = memoria[endereco][7];
        OpCode1 = memoria[endereco][6];
        OpCode0 = memoria[endereco][5];
        reg1 = memoria[endereco][4:3];
        reg2 = memoria[endereco][2:1];
        funct = memoria[endereco][0];
        imediato = memoria[endereco][2:0];
    end
endmodule
/*
Banco de Registradores
*/
module BancoDeRegistradores(
    input [7:0] reEntrada,
    input Clock, Reset,
    input RegWrite,
    input [1:0] Reg1, Reg2,
    input [7:0] dadoEscrever,
    output reg [7:0] dado1, dado2, reSaida
    );
    reg [7:0] registradores [3:0]; // 4 registradores de uso geral
    always @(*) begin // Leitura assíncrona dos registradores
	dado1 = registradores[Reg1];
	dado2 = registradores[Reg2];
    end
    always @(posedge Clock or posedge Reset) begin // Escrita e controle síncronos
	if (Reset) begin
	    registradores[0] <= 8'b0; // Zera os registradores gerais
	    registradores[1] <= 8'b0;
	    registradores[2] <= 8'b0;
	    registradores[3] <= 8'b0;
	    reSaida <= 8'b11111100; // Inicializa re (-4)
	end else begin // Escrita nos registradores indexados
	    if (RegWrite) registradores[Reg1] <= dadoEscrever;
	    reSaida <= reEntrada;
	end
    end
endmodule
/*
Ula
*/
module Ula(
    input [7:0] A, B, 
    input [7:0] reEntrada,
    input ALUOp1 ,ALUOp0, funct, RegWrite,
    output reg [7:0] S,
    output reg [7:0] reSaida,
    output reg zero, 
    output reg overflow
    );
    // add e sub
    wire [7:0] saidaAddSub; 
    wire overflowAddSub;
    somador8bits s8bAddSub(.A(A), .B(B), .CarryIn(funct), .sinal(funct), .S(saidaAddSub), .CarryOut(overflowAddSub));
    // addi
    wire [7:0] saidaAddi; 
    wire overflowAddi;
    somador8bits s8bAddi(.A(A), .B(B), .CarryIn(1'b0), .sinal(1'b0),.S(saidaAddi), .CarryOut(overflowAddi));
    // mult
    wire [7:0] saidaMult;
    wire overflowMult;
    multiplicador8bits m8bMult(.A(A), .B(B), .S(saidaMult), .overflow(overflowMult));
    // beq e slt
    wire [7:0] saidaSub;
    wire overflowSub;
    somador8bits s8bD(.A(A), .B(B), .CarryIn(1'b1), .sinal(1'b1), .S(saidaSub), .CarryOut(overflowSub));
    wire beq = ~(|saidaSub);
    wire slt = saidaSub[7];
    // result
    wire [7:0] saidaResult;
    wire overflowResult;
    somador8bits s8bR(.A(B), .B(reEntrada), .CarryIn(1'b1), .sinal(1'b1),.S(saidaResult), .CarryOut(overflowResult));
    wire result = ~(|saidaResult);
    always @(*) begin
        case ({ALUOp1 ,ALUOp0})
            2'b00: begin // add e sub
		if(RegWrite == 1'b1) begin
		    S = saidaAddSub;
		    zero = 1'b0;
		    reSaida = reEntrada;
		    overflow = overflowAddSub;
		end else begin // result
		    S = 8'b0;
		    zero = result ? 1'b1 : 1'b0;
		    reSaida = 8'b11111100;
		    overflow = overflowResult;
		end			
            end
            2'b01: begin // addi
	        S = saidaAddi;
		zero = 1'b0;
		reSaida = reEntrada;
		overflow = overflowAddi;
	    end
            2'b10: begin // mult
		S = saidaMult;
		zero = 1'b0;
		reSaida = reEntrada;
		overflow = overflowMult;
            end
            2'b11: begin // beq e slt
		S = 8'b0;
		zero = 1'b0;
		if(funct == 1'b0) reSaida = beq ? 8'b00000001 : 8'b0;
		else reSaida = slt ? 8'b00000001 : 8'b0;
		overflow = overflowSub;
            end
        endcase
    end
endmodule
/*
Processador
*/
module nRisc(
    input Clock,
    input Reset
    );
    // wires pc
    wire [7:0]enderecoPC, auxEndereco;
    wire auxCarryOut;
    somador8bits pc_1(.A(enderecoPC),
	.B(8'b00000001), 
	.sinal(1'b0),
	.CarryIn(1'b0), 
	.S(auxEndereco), 
	.CarryOut(auxCarryOut)
    );
    wire [7:0]endereco = NextOp ? auxEndereco : (Jump ? dado1 : (Branch & zero ? dado1 : auxEndereco));
    // wires da memória de instruções
    wire OpCode2, OpCode1, OpCode0, funct;
    wire [1:0]reg1, reg2;
    wire [2:0]imediato;
    // wires banco de registradores
    wire [7:0]reSaida_BANCO, reSaida_ULA, dado1, dado2;
    // wire ula
    wire [7:0]saidaUla;
    wire zero, overflow;
    // wires memória de dados
    wire [7:0]dadoLido;
    // wires do controle
    wire ALUSrc, MemToReg, RegWrite, MemRead, MemWrite;
    wire Branch, Jump, NextOp, End, ALUOp1, ALUOp0;
    Controle control_(
	.Op2(OpCode2), // começo input
	.Op1(OpCode1),
	.Op0(OpCode0),
	.funct(funct),
	.reset(Reset),
	.ALUSrc(ALUSrc),
	.MemToReg(MemToReg), // começo output
	.RegWrite(RegWrite),
	.MemRead(MemRead),
	.MemWrite(MemWrite),
	.Branch(Branch),
	.Jump(Jump),
	.NextOp(NextOp),
	.End(End),
	.ALUOp1(ALUOp1),
	.ALUOp0(ALUOp0)
    );
    Pc pc_(
        .endereco(endereco), // começo input
        .End(End),
        .Reset(Reset),
        .Clock(Clock),
        .enderecoSaida(enderecoPC) // começo output
    );
    MemoriaData memData_(
	.endereco(dado2), // começo input
	.dadoEscrever(dado1),
	.MemRead(MemRead),
	.MemWrite(MemWrite),
	.Clock(Clock),
	.dadoLido(dadoLido) // começo output
    );
    MemoriaInstrucoes memInst_(
	.endereco(enderecoPC), // input
	.OpCode2(OpCode2), // começo output
	.OpCode1(OpCode1),
	.OpCode0(OpCode0), 
	.funct(funct),
	.reg1(reg1),
	.reg2(reg2),	
	.imediato(imediato)
    );
    BancoDeRegistradores bancoReg_(
	.reEntrada(reSaida_ULA), //começo input
	.Clock(Clock),
	.Reset(Reset),
	.RegWrite(RegWrite),
	.Reg1(reg1), 
	.Reg2(reg2),
	.dadoEscrever(MemToReg ? dadoLido : saidaUla),
	.dado1(dado1), // começo output
	.dado2(dado2),
	.reSaida(reSaida_BANCO)
    );
    wire [7:0] imediatoExtendido = {{5{imediato[2]}}, imediato}; // Extensor de sinal
    Ula ula_(
	.A(dado1), // começo input
	.B(ALUSrc ? imediatoExtendido : dado2), 
	.reEntrada(reSaida_BANCO),
	.ALUOp1(ALUOp1),
	.ALUOp0(ALUOp0),
	.funct(funct),
	.RegWrite(RegWrite),
	.S(saidaUla), // começo output
	.reSaida(reSaida_ULA),
	.zero(zero), 
	.overflow(overflow)
    );
endmodule
