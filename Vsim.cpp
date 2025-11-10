#include <iostream>
#include <ostream>
#include <fstream>
#include <string>
#include <vector>
#include <bitset>
#include <memory>
#include <unordered_map>
#include <queue>
/*
 * The code below was made before I saw we needed one to submit only 1 file
 * so there is an element of polymorphism here with Instruction class and
 * Program class. I liked how I had originally designed it so I stuck with it
 */

#define PREISSUEMAX = 4
#define PREALU1MAX = 2
#define PREALU2MAX = 1
#define PREALU3MAX = 1
#define PREMEMMAX = 1
#define POSTMEMMAX = 1
#define POSTALU2MAX = 1
#define POSTALU3MAX = 1


using namespace std;

// ================== Globals ==================

static unordered_map<string,string> OPCODES_1 = {
    {"00000", "beq"},
    {"00001", "bne"},
    {"00010", "blt"},
    {"00011", "sw"}
};

static unordered_map<string,string> OPCODES_2 = {
    {"00000", "add"},
    {"00001", "sub"},
    {"00010", "and"},
    {"00011", "or"}
};

static unordered_map<string,string> OPCODES_3 = {
    {"00000", "addi"},
    {"00001", "andi"},
    {"00010", "ori"},
    {"00011", "slli"},
    {"00100", "srai"},
    {"00101", "lw"}
};

static unordered_map<string,string> OPCODES_4 = {
    {"00000", "jal"},
    {"11111", "break"}
};

enum RegisterRole{
    DESTINATION,
    SOURCE,
    TARGET,
    FREE
};

unordered_map<int, string> stringInstruction;
int memoryStart;

bool runningPipeline;

// ================== Instruction Class ==================

// all Instructions have a bit field and a category
// each category is a child of Instruction so they can
// all be put into an instruction vector

class Instruction {
public:
    explicit Instruction(string binary) : raw_(move(binary)) {}
    virtual ~Instruction() = default;

    enum class Category { CAT1, CAT2, CAT3, CAT4 };
    virtual Category category() const = 0;

protected:
    string raw_;
};

class Cat1Instruction : public Instruction {
public:
    explicit Cat1Instruction(string binary) : Instruction(binary) {
        bits.inst   = bitset<32>(binary);
        bits.imm115 = bitset<7>(binary.substr(0, 7));
        bits.rs2    = bitset<5>(binary.substr(7, 5));
        bits.rs1    = bitset<5>(binary.substr(12,5));
        bits.func3  = bitset<3>(binary.substr(17,3));
        bits.imm40  = bitset<5>(binary.substr(20,5));
        bits.opcode = bitset<5>(binary.substr(25,5));
        bits.cat    = bitset<2>(binary.substr(30,2));
    }
    Category category() const override { return Category::CAT1; }

    struct Field {
        bitset<32> inst;
        bitset<7> imm115;
        bitset<5> rs2;
        bitset<5> rs1;
        bitset<3> func3;
        bitset<5> imm40;
        bitset<5> opcode;
        bitset<2> cat;
    } bits;

    Field getBits() { return bits; }
};

class Cat2Instruction : public Instruction {
public:
    explicit Cat2Instruction(string binary) : Instruction(binary) {
        bits.inst   = bitset<32>(binary);
        bits.func7  = bitset<7>(binary.substr(0, 7));
        bits.rs2    = bitset<5>(binary.substr(7, 5));
        bits.rs1    = bitset<5>(binary.substr(12,5));
        bits.func3  = bitset<3>(binary.substr(17,3));
        bits.rd     = bitset<5>(binary.substr(20,5));
        bits.opcode = bitset<5>(binary.substr(25,5));
        bits.cat    = bitset<2>(binary.substr(30,2));
    }
    Category category() const override { return Category::CAT2; }
    struct Field {
        bitset<32> inst;
        bitset<7> func7;
        bitset<5> rs2;
        bitset<5> rs1;
        bitset<3> func3;
        bitset<5> rd;
        bitset<5> opcode;
        bitset<2> cat;
    } bits;

    Field getBits() { return bits; }  // FIX: actually return the stored bits
};

class Cat3Instruction : public Instruction {
public:
    explicit Cat3Instruction(string binary) : Instruction(binary) {
        bits.inst   = bitset<32>(binary);
        bits.imm110 = bitset<12>(binary.substr(0,12));
        bits.rs1    = bitset<5>(binary.substr(12,5));
        bits.func3  = bitset<3>(binary.substr(17,3));
        bits.rd     = bitset<5>(binary.substr(20,5));
        bits.opcode = bitset<5>(binary.substr(25,5));
        bits.cat    = bitset<2>(binary.substr(30,2));
    }
    Category category() const override { return Category::CAT3; }
    struct Field {
        bitset<32> inst;
        bitset<12> imm110;
        bitset<5>  rs1;
        bitset<3>  func3;
        bitset<5>  rd;
        bitset<5>  opcode;
        bitset<2>  cat;
    } bits;

    Field getBits() { return bits; }  // FIX
};

class Cat4Instruction : public Instruction {
public:
    explicit Cat4Instruction(string binary) : Instruction(binary) {
        bits.inst   = bitset<32>(binary);
        bits.imm190 = bitset<20>(binary.substr(0,20));
        bits.rd     = bitset<5>(binary.substr(20,5));
        bits.opcode = bitset<5>(binary.substr(25,5));
        bits.cat    = bitset<2>(binary.substr(30,2));
    }
    Category category() const override { return Category::CAT4; }

    struct Field {
        bitset<32> inst;
        bitset<20> imm190;
        bitset<5>  rd;
        bitset<5>  opcode;
        bitset<2>  cat;
    } bits;

    Field getBits() { return bits; }  // FIX
};

// ================== Program singleton ==================

// holds all important info of the running program, such as the instructions
// the registers, and the memoty

//@TODO Add queues to here for pipeline
class Program {
public:
    static Program& instance() {
        static Program program;
        return program;
    }

    Program(const Program&) = delete;
    Program& operator=(const Program&) = delete;
    Program(Program&&) = delete;
    Program& operator=(Program&&) = delete;

    vector<unique_ptr<Instruction>>& getInstructions() { return instructions; }
    vector<int>& getRegisters() { return registers; }
    vector<RegisterRole> getRegisterRoles() {return registerRoles; }
    vector<pair<int, bitset<32>>>& getMemory() { return memory; }
    int getCycle() {return cycle; }
    int getPC() {return programCounter; }
    queue<unique_ptr<Instruction>>& getInstQueue() { return inst_queue; }
    void initialize(){
        registers.assign(32, 0);
        cycle = 1;
        programCounter = 256;
    }
    void addInstruction(unique_ptr<Instruction> instruction) {
        instructions.push_back(move(instruction));
        inst_queue.push(static_cast<unique_ptr<Instruction>>(instructions.back().get()));
    }
    void addMemory(int address, bitset<32> data) { memory.emplace_back(address, data);}

    void incrementCycle(int inc) {cycle += inc;}
    void modifyPC(int mod) {programCounter += mod; }

    bitset<32> getValueAtMem(int address){
        for (auto& it : memory) {
            if (it.first == address) { return it.second; }
        }
    }

    void modifyMemory(int address, int data) {
        for (auto& it : memory) {
            if (it.first == address) { it.second = data; }
        }
    }

    void setWaiting(auto instruction){
        IFwaiting = unique_ptr<Instruction>(instruction);
    }
    void setExecuted(auto instruction){
        IFexecuted = unique_ptr<Instruction>(instruction);
    }

    // These biffered ones are since I have the pipeline run in a way where eahc functional unit happens one after another
    // I need ot buffer adding the queues to ensure we dont go through everything in one cycle.
    deque<unique_ptr<Instruction>> bufferedPreIssueQueue;

    deque<unique_ptr<Instruction>> bufferedPreALU1Queue;
    deque<unique_ptr<Instruction>> bufferedPreALU2Queue;
    deque<unique_ptr<Instruction>> bufferedPreALU3Queue;

    deque<pair<unique_ptr<Instruction>, int>>    bufferedPreMemQueue;
    deque<pair<unique_ptr<Instruction>, pair<int, int>>> bufferedPostMemQueue;
    deque<pair<unique_ptr<Instruction>, int>>    bufferedPostALU2Queue;
    deque<pair<unique_ptr<Instruction>, int>>    bufferedPostALU3Queue;

    deque<unique_ptr<Instruction>>    preIssueQueue;

    deque<unique_ptr<Instruction>>    preALU1Queue;
    deque<unique_ptr<Instruction>>    preALU2Queue;
    deque<unique_ptr<Instruction>>    preALU3Queue;

    deque<pair<unique_ptr<Instruction>, int>>    preMemQueue;
    deque<pair<unique_ptr<Instruction>, pair<int, int>>>    postMemQueue;
    deque<pair<unique_ptr<Instruction>, int>>    postALU2Queue;
    deque<pair<unique_ptr<Instruction>, int>>    postALU3Queue;

    unique_ptr<Instruction>           IFwaiting;
    unique_ptr<Instruction>           IFexecuted;

private:
    Program() = default;
    ~Program() = default;

    vector<unique_ptr<Instruction>>   instructions;
    queue<unique_ptr<Instruction>>    inst_queue;
    vector<int>                       registers;
    vector<RegisterRole>              registerRoles;
    vector<pair<int, bitset<32>>>     memory;

    int                               cycle;
    int                               programCounter;
};


// ================== Running function helpers ==================

// parses the instsructions from the input txt and into the
// instruction vector
static bool parseInstruction(const string& line) {
    const string catBits = line.substr(30);

    if (catBits.find("00") != string::npos) {
        Program::instance().addInstruction(make_unique<Cat1Instruction>(line));
    }
    else if (catBits.find("01") != string::npos) {
        Program::instance().addInstruction(make_unique<Cat2Instruction>(line));
    }
    else if (catBits.find("10") != string::npos) {
        Program::instance().addInstruction(make_unique<Cat3Instruction>(line));
    }
    else if (catBits.find("11") != string::npos) {
        Program::instance().addInstruction(make_unique<Cat4Instruction>(line));

        auto* inst = dynamic_cast<Cat4Instruction*>(
            Program::instance().getInstructions().back().get());

        if (inst && OPCODES_4[inst->getBits().opcode.to_string()] == "break") {
            return true;
        }
    }
    return false;
}

// returns the 2's compliment integer
int32_t singedImm(unsigned long bits, int size){
    return (int32_t)(bits << (32 - size)) >> (32 - size);
}

// writes to the dissasembly txt
void writeDisassembly() {
    ofstream out("disassembly.txt");
    int memLocation = 256;

    for (auto& instruction : Program::instance().getInstructions()) {
        string instFormat;
        switch (instruction->category()) {
            case Instruction::Category::CAT1: {
                auto* cat1 = dynamic_cast<Cat1Instruction*>(instruction.get());

                string opc = cat1->bits.opcode.to_string();
                string mnem = OPCODES_1[opc];

                instFormat += cat1->bits.inst.to_string() + "\t";
                instFormat += to_string(memLocation) + "\t";
                instFormat += mnem + " ";
                instFormat += "x" + to_string(cat1->bits.rs1.to_ulong()) + ", ";

                if (mnem == "sw") {
                    bitset<12> mem(cat1->bits.imm115.to_string() + cat1->bits.imm40.to_string());
                    instFormat += to_string(mem.to_ulong());
                    instFormat += "(x" + to_string(cat1->bits.rs2.to_ulong()) + ")\n";
                } else {
                    instFormat += "x" + to_string(cat1->bits.rs2.to_ulong()) + ", ";
                    instFormat += "#" + to_string((bitset<12>(cat1->bits.imm115.to_string() + cat1->bits.imm40.to_string())).to_ulong()) + "\n";
                }

                out << instFormat;
                break;
            }

            case Instruction::Category::CAT2: {
                auto* cat2 = dynamic_cast<Cat2Instruction*>(instruction.get());

                string opc  = cat2->bits.opcode.to_string();
                string mnem = OPCODES_2[opc];

                instFormat += cat2->bits.inst.to_string() + "\t";
                instFormat += to_string(memLocation) + "\t";
                instFormat += mnem + " ";

                auto rd  = cat2->bits.rd.to_ulong();
                auto rs1 = cat2->bits.rs1.to_ulong();
                auto rs2 = cat2->bits.rs2.to_ulong();

                instFormat += "x" + to_string(rd)
                              + ", x" + to_string(rs1)
                              + ", x" + to_string(rs2) + "\n";

                out << instFormat;
                break;
            }

            case Instruction::Category::CAT3: {
                auto* cat3 = dynamic_cast<Cat3Instruction*>(instruction.get());

                const string opc  = cat3->bits.opcode.to_string();
                const string mnem = OPCODES_3[opc];

                instFormat += cat3->bits.inst.to_string() + "\t";
                instFormat += to_string(memLocation) + "\t";
                instFormat += mnem + " ";
                instFormat += "x" + to_string(cat3->bits.rd.to_ulong()) + ", ";

                if (mnem == "lw") {
                    instFormat += to_string(cat3->bits.imm110.to_ulong());
                    instFormat += "(x" + to_string(cat3->bits.rs1.to_ulong()) + ")\n";
                } else {
                    instFormat += "x" + to_string(cat3->bits.rs1.to_ulong()) + ", ";
                    instFormat += "#" + to_string(singedImm(cat3->bits.imm110.to_ulong(), cat3->bits.imm110.size())) + "\n";
                }

                out << instFormat;
                break;
            }

            case Instruction::Category::CAT4: {
                auto* cat4 = dynamic_cast<Cat4Instruction*>(instruction.get());

                const string opc  = cat4->bits.opcode.to_string();
                const string mnem = OPCODES_4[opc];

                instFormat += cat4->bits.inst.to_string() + "\t";
                instFormat += to_string(memLocation) + "\t";

                if (mnem == "jal") {
                    instFormat += mnem + " ";
                    instFormat += "x" + to_string(cat4->bits.rd.to_ulong()) + ", ";
                    instFormat += "#" + to_string(singedImm((cat4->bits.imm190.to_ulong()),cat4->bits.imm190.size()))
                                  + "\n";
                } else {
                    instFormat += mnem + "\n";
                }
                out << instFormat;
                break;
            }

            default:
                break;
        }
        stringInstruction.insert({memLocation, instFormat.substr(33)});
        memLocation+= 4;
    }

    for(auto& memory : Program::instance().getMemory()){
        out << memory.second << "\t" << memory.first << "\t" << singedImm(memory.second.to_ulong(), memory.second.size()) << "\n";
    }
    out.close();
}

//writes to the simulation txt
void writeSimulation(int mem, int cycle, ofstream& out){
    out << "--------------------\n";
    out << "Cycle " << cycle << ":\t" <<  stringInstruction[mem];

    auto& registers = Program::instance().getRegisters();
    auto& memory = Program::instance().getMemory();

    string Registers = "Registers";
    string Data = "Data";

    for(int i = 0; i < registers.size(); i++){
        if(i%8 == 0){
            Registers += "\nx" + ((i < 10 ? "0" : "") + std::to_string(i)) + ":";
        }
        Registers += "\t" + to_string(registers[i]);
    }
    out << Registers << "\n";

    for(int i = 0; i < memory.size(); i++){
        if(i%8 == 0){
            Data += "\n" + to_string(memoryStart + i*4) + ":";
        }
        Data += "\t" + to_string(singedImm(memory[i].second.to_ulong(),memory[i].second.size()));
    }
    out << Data;
}

// disassembles the code from the input file into individual lines so
// parse instruction can handle the instruction
void disassemble(ifstream& file) {
    int address = 256;
    string line;
    while (getline(file, line, '\n')) {
        if (parseInstruction(line)) {
            address += 4;
            break;
        }
        address += 4;
    }
    memoryStart = address;

    while (getline(file, line)) {
        Program::instance().addMemory(address, bitset<32>(line));
        address += 4;
    }
}

// performs the logic for the instructions
void runSimulation(){
    ofstream sim("simulation.txt");

    int programCounter = 256;
    int pcAdd;
    int cycle = 1;
    bool  running = true;


    auto &instruction = Program::instance().getInstructions();
    auto &registers = Program::instance().getRegisters();
    auto &memory = Program::instance().getMemory();

    while (running) {
        switch ((instruction[(programCounter - 256) / 4]).get()->category()) {
            case Instruction::Category::CAT1: {
                auto cat1 = (Cat1Instruction *) (instruction[(programCounter - 256) / 4]).get();
                int rs1 = (int)cat1->bits.rs1.to_ulong();
                int rs2 = (int)cat1->bits.rs2.to_ulong();
                bitset<12> mem = bitset<12>(cat1->bits.imm115.to_string() + cat1->bits.imm40.to_string());

                if (OPCODES_1[cat1->bits.opcode.to_string()] == "beq") {
                    if (registers[rs1] == registers[rs2]) {
                        pcAdd = (int)(mem.to_ulong() << 1);
                    }
                    else{
                        pcAdd = 4;
                    }
                }
                else if (OPCODES_1[cat1->bits.opcode.to_string()] == "bne") {
                    if (registers[rs1] != registers[rs2]) {
                        pcAdd = (int)(mem.to_ulong() << 1);
                    }
                    else{
                        pcAdd = 4;
                    }
                }
                else if (OPCODES_1[cat1->bits.opcode.to_string()] == "blt") {
                    if (registers[rs1] < registers[rs2]) {
                        pcAdd = (int)(mem.to_ulong() << 1);
                    }
                    else{
                        pcAdd = 4;
                    }
                }
                else if (OPCODES_1[cat1->bits.opcode.to_string()] == "sw") {
                    int memToModify = mem.to_ulong() + registers[rs2];
                    int value = registers[rs1];
                    Program::instance().modifyMemory(memToModify, value);
                    pcAdd = 4;
                }
                break;
            }
            case Instruction::Category::CAT2: {
                auto cat2 = (Cat2Instruction *) (instruction[(programCounter - 256) / 4]).get();
                int rs1 = (int)cat2->bits.rs1.to_ulong();
                int rs2 = (int)cat2->bits.rs2.to_ulong();
                int rd =  (int)cat2->bits.rd.to_ulong();

                if (OPCODES_2[cat2->bits.opcode.to_string()] == "add") {
                    registers[rd] = registers[rs1] + registers[rs2];
                }
                else if (OPCODES_2[cat2->bits.opcode.to_string()] == "sub") {
                    registers[rd] = registers[rs1] - registers[rs2];
                }
                else if (OPCODES_2[cat2->bits.opcode.to_string()] == "and") {
                    registers[rd] = (int)(bitset<32>(registers[rs1]) & bitset<32>(registers[rs2])).to_ulong();
                }
                else if (OPCODES_2[cat2->bits.opcode.to_string()] == "or") {
                    registers[rd] = (int)(bitset<32>(registers[rs1]) | bitset<32>(registers[rs2])).to_ulong();
                }
                pcAdd = 4;
                break;
            }
            case Instruction::Category::CAT3: {
                auto cat3 = (Cat3Instruction *) (instruction[(programCounter - 256) / 4]).get();
                int rs1 = (int)cat3->bits.rs1.to_ulong();
                int rd = (int)cat3->bits.rd.to_ulong();
                int imm =  singedImm(cat3->bits.imm110.to_ulong(), cat3->bits.imm110.size());

                if (OPCODES_3[cat3->bits.opcode.to_string()] == "addi") {
                    registers[rd] = registers[rs1] + imm;
                }
                else if (OPCODES_3[cat3->bits.opcode.to_string()] == "andi") {
                    registers[rd] = (int)(bitset<32>(registers[rs1]) & bitset<32>(imm)).to_ulong();
                }
                else if (OPCODES_3[cat3->bits.opcode.to_string()] == "ori") {
                    registers[rd] = (int)(bitset<32>(registers[rs1]) | bitset<32>(imm)).to_ulong();
                }
                else if (OPCODES_3[cat3->bits.opcode.to_string()] == "slli") {
                    registers[rd] = (int)(bitset<32>(registers[rs1]) << imm).to_ulong();
                }
                else if (OPCODES_3[cat3->bits.opcode.to_string()] == "srai") {
                    registers[rd] = (int)(bitset<32>(registers[rs1]) >> imm).to_ulong();
                }
                else if (OPCODES_3[cat3->bits.opcode.to_string()] == "lw") {
                    auto test = (int)Program::instance().getValueAtMem(imm + registers[rs1]).to_ulong();
                    registers[rd]= (int)Program::instance().getValueAtMem(imm + registers[rs1]).to_ulong();
                }

                pcAdd = 4;
                break;
            }
            case Instruction::Category::CAT4: {
                auto cat4 = (Cat4Instruction *) (instruction[(programCounter - 256) / 4]).get();
                int rd =  (int)cat4->bits.rd.to_ulong();

                if(OPCODES_4[cat4->bits.opcode.to_string()] == "jal"){
                    pcAdd = singedImm((cat4->bits.imm190 << 1).to_ulong(), (cat4->bits.imm190 << 1).size());
                    registers[rd] = programCounter + 4;
                }
                else if(OPCODES_4[cat4->bits.opcode.to_string()] == "break"){
                    running = false;
                }
                break;
            }
        }


        writeSimulation(programCounter, cycle, sim);
        if (running) {
            sim << "\n";
        }
        programCounter += pcAdd;
        cycle++;
    }
    sim.close();
}

// ------------- Functional Units -------------

void IF(){
    auto &instruction = Program::instance().getInstructions();
    auto &registers = Program::instance().getRegisters();
    auto registerState= Program::instance().getRegisterRoles();
    auto &memory = Program::instance().getMemory();

    int i = 0;
    while (i < 2){
        int programCounter = Program::instance().getCycle();
        switch ((instruction[(programCounter - 256) / 4]).get()->category()) {
            case Instruction::Category::CAT1: {
                auto cat1 = (Cat1Instruction *) (instruction[(programCounter - 256) / 4]).get();
                int rs1 = (int) cat1->bits.rs1.to_ulong();
                int rs2 = (int) cat1->bits.rs2.to_ulong();
                bitset<12> mem = bitset<12>(cat1->bits.imm115.to_string() + cat1->bits.imm40.to_string());

                if (OPCODES_1[cat1->bits.opcode.to_string()] == "beq" or
                    OPCODES_1[cat1->bits.opcode.to_string()] == "bne" or
                    OPCODES_1[cat1->bits.opcode.to_string()] == "blt") {
                    if (registerState[rs1] == FREE && registerState[rs2] == FREE) {
                        if (OPCODES_1[cat1->bits.opcode.to_string()] == "beq") {
                            if (registers[rs1] == registers[rs2]) {
                                Program::instance().modifyPC((int) (mem.to_ulong() << 1));
                            } else {
                                Program::instance().modifyPC(4);
                            }
                        }
                        else if (OPCODES_1[cat1->bits.opcode.to_string()] == "bne") {
                            if (registers[rs1] != registers[rs2]) {
                                Program::instance().modifyPC((int) (mem.to_ulong() << 1));
                            } else {
                                Program::instance().modifyPC(4);
                            }
                        }
                        else if (OPCODES_1[cat1->bits.opcode.to_string()] == "blt") {
                            if (registers[rs1] < registers[rs2]) {
                                Program::instance().modifyPC((int) (mem.to_ulong() << 1));
                            } else {
                                Program::instance().modifyPC(4);
                            }
                        }
                        Program::instance().instance().setExecuted(cat1);
                    }
                    else{
                        Program::instance().instance().setWaiting(cat1);
                        i = 2;
                        break;
                    }
                }

                else{
                    if(Program::instance().preIssueQueue.size() + Program::instance().bufferedPreIssueQueue.size() < 4){
                        Program::instance().bufferedPreIssueQueue.push_back(unique_ptr<Instruction>(cat1));
                    }
                    else{
                        i = 2;
                        break;
                    }
                }
                break;
            }
            case Instruction::Category::CAT2: {
                auto cat2 = (Cat2Instruction *) (instruction[(programCounter - 256) / 4]).get();
                if(Program::instance().preIssueQueue.size() + Program::instance().bufferedPreIssueQueue.size() < 4){
                    Program::instance().bufferedPreIssueQueue.push_back(unique_ptr<Instruction>(cat2));
                }
                else{
                    i = 2;
                    break;
                }
                break;
            }
            case Instruction::Category::CAT3: {
                auto cat3 = (Cat3Instruction *) (instruction[(programCounter - 256) / 4]).get();
                if(Program::instance().preIssueQueue.size() + Program::instance().bufferedPreIssueQueue.size() < 4){
                    Program::instance().bufferedPreIssueQueue.push_back(unique_ptr<Instruction>(cat3));
                }
                else{
                    i = 2;
                    break;
                }
                break;
            }
            case Instruction::Category::CAT4: {
                auto cat4 = (Cat4Instruction *) (instruction[(programCounter - 256) / 4]).get();
                if(Program::instance().preIssueQueue.size() + Program::instance().bufferedPreIssueQueue.size() < 4){
                    Program::instance().bufferedPreIssueQueue.push_back(unique_ptr<Instruction>(cat4));
                }
                else{
                    i = 2;
                    break;
                }
                break;
            }
        }
    }
        i++;
}


void Isssue(){
    auto registerState= Program::instance().getRegisterRoles();

    for(auto &inst : Program::instance().preIssueQueue){
        switch ((inst->category())) {
            case Instruction::Category::CAT1: {
                auto cat1 = (Cat1Instruction *)(inst.get());
                int rs1 = (int)cat1->bits.rs1.to_ulong();
                int rs2 = (int)cat1->bits.rs2.to_ulong();
                bitset<12> mem = bitset<12>(cat1->bits.imm115.to_string() + cat1->bits.imm40.to_string());

                if (OPCODES_1[cat1->bits.opcode.to_string()] == "sw" &&
                    registerState[rs1] == FREE && registerState[rs2] == FREE) {
                    Program::instance().bufferedPreALU1Queue.push_back(unique_ptr<Instruction>(cat1));
                    Program::instance().preIssueQueue.pop_front();
                    registerState[rs1] = SOURCE;
                    registerState[rs2] = SOURCE;
                }
                break;
            }
            case Instruction::Category::CAT2: {
                auto cat2 = (Cat2Instruction *)(inst.get());
                int rs1 = (int)cat2->bits.rs1.to_ulong();
                int rs2 = (int)cat2->bits.rs2.to_ulong();
                int rd =  (int)cat2->bits.rd.to_ulong();

                if ((OPCODES_2[cat2->bits.opcode.to_string()] == "add" || OPCODES_2[cat2->bits.opcode.to_string()] == "sub")&&
                    registerState[rs1] == FREE && registerState[rs2] == FREE && registerState[rd] == FREE) {
                    Program::instance().bufferedPreALU2Queue.push_back(unique_ptr<Instruction>(cat2));
                    Program::instance().preIssueQueue.pop_front();
                    registerState[rs1] = SOURCE;
                    registerState[rs2] = SOURCE;
                    registerState[rd] = DESTINATION;
                }
                else if ((OPCODES_2[cat2->bits.opcode.to_string()] == "or")&&
                    registerState[rs1] == FREE && registerState[rs2] == FREE && registerState[rd] == FREE) {
                    Program::instance().bufferedPreALU3Queue.push_back(unique_ptr<Instruction>(cat2));
                    Program::instance().preIssueQueue.pop_front();
                    registerState[rs1] = SOURCE;
                    registerState[rs2] = SOURCE;
                    registerState[rd] = DESTINATION;
                }

                break;
            }
            case Instruction::Category::CAT3: {
                auto cat3 = (Cat3Instruction *)(inst.get());
                int rs1 = (int)cat3->bits.rs1.to_ulong();
                int rd = (int)cat3->bits.rd.to_ulong();
                int imm =  singedImm(cat3->bits.imm110.to_ulong(), cat3->bits.imm110.size());

                if ((OPCODES_3[cat3->bits.opcode.to_string()] == "addi")&&
                    registerState[rs1] == FREE && registerState[rd] == FREE) {
                    Program::instance().bufferedPreALU2Queue.push_back(unique_ptr<Instruction>(cat3));
                    Program::instance().preIssueQueue.pop_front();
                    registerState[rs1] = SOURCE;
                    registerState[rd] = DESTINATION;
                }
                else if ((OPCODES_3[cat3->bits.opcode.to_string()] == "andi" || OPCODES_3[cat3->bits.opcode.to_string()] == "ori"
                    || OPCODES_3[cat3->bits.opcode.to_string()] == "slli" || OPCODES_3[cat3->bits.opcode.to_string()] == "srai") && registerState[rs1] == FREE
                    && registerState[rs1] == FREE && registerState[rd]  == FREE) {
                    Program::instance().bufferedPreALU3Queue.push_back(unique_ptr<Instruction>(cat3));
                    Program::instance().preIssueQueue.pop_front();
                    registerState[rs1] = SOURCE;
                    registerState[rd] = DESTINATION;
                }
                else if ((OPCODES_3[cat3->bits.opcode.to_string()] == "lw")&&
                    registerState[rs1] == FREE && registerState[rd] == FREE ) {
                    Program::instance().bufferedPreALU1Queue.push_back(unique_ptr<Instruction>(cat3));
                    Program::instance().preIssueQueue.pop_front();
                    registerState[rs1] = SOURCE;
                    registerState[rd] = DESTINATION;
                }

                break;
            }
            case Instruction::Category::CAT4: {
                break;
            }

        }
    }
}

void ALU(){

}

void ALU1(){
    auto &inst = Program::instance().preALU1Queue.front();
    auto &instruction = Program::instance().getInstructions();
    auto &registers = Program::instance().getRegisters();
    auto &memory = Program::instance().getMemory();

    switch (inst.get()->category()) {
        case Instruction::Category::CAT1: {
            auto cat1 = (Cat1Instruction *) (inst.get());
            int rs1 = (int)cat1->bits.rs1.to_ulong();
            int rs2 = (int)cat1->bits.rs2.to_ulong();
            int memToModify;
            bitset<12> mem = bitset<12>(cat1->bits.imm115.to_string() + cat1->bits.imm40.to_string());

            if (OPCODES_1[cat1->bits.opcode.to_string()] == "sw") {
                int memToModify = mem.to_ulong() + registers[rs2];
                Program::instance().bufferedPreMemQueue.emplace_back(cat1, memToModify);
                Program::instance().preALU1Queue.pop_front();
            }
            break;
        }
        case Instruction::Category::CAT3: {
            auto cat3 = (Cat3Instruction *) (inst.get());
            int rs1 = (int)cat3->bits.rs1.to_ulong();
            int rd = (int)cat3->bits.rd.to_ulong();
            int imm =  singedImm(cat3->bits.imm110.to_ulong(), cat3->bits.imm110.size());

            if (OPCODES_3[cat3->bits.opcode.to_string()] == "lw") {
                int memToModify = imm + registers[rs1];
                Program::instance().bufferedPreMemQueue.emplace_back(cat3, memToModify);
                Program::instance().preALU1Queue.pop_front();
            }
            break;
        }
    }
}

void ALU2(){
    auto &inst = Program::instance().preALU2Queue.front();
    auto &instruction = Program::instance().getInstructions();
    auto &registers = Program::instance().getRegisters();
    auto &memory = Program::instance().getMemory();

    switch ((inst->category())) {
        case Instruction::Category::CAT2: {
            auto cat2 = (Cat2Instruction *)(inst.get());
            int rs1 = (int)cat2->bits.rs1.to_ulong();
            int rs2 = (int)cat2->bits.rs2.to_ulong();
            int result;

            if (OPCODES_2[cat2->bits.opcode.to_string()] == "add") {
                result = registers[rs1] + registers[rs2];
            }
            else if (OPCODES_2[cat2->bits.opcode.to_string()] == "sub") {
                result = registers[rs1] - registers[rs2];
            }
            Program::instance().bufferedPostALU2Queue.emplace_back(cat2, result);
            Program::instance().preALU2Queue.pop_front();

            break;
        }
        case Instruction::Category::CAT3: {
            auto cat3 = (Cat3Instruction *)(inst.get());
            int rs1 = (int)cat3->bits.rs1.to_ulong();
            int rd = (int)cat3->bits.rd.to_ulong();
            int imm =  singedImm(cat3->bits.imm110.to_ulong(), cat3->bits.imm110.size());

            if (OPCODES_3[cat3->bits.opcode.to_string()] == "addi") {
                int result = registers[rs1] + imm;
                Program::instance().bufferedPostALU3Queue.emplace_back(cat3, result);
                Program::instance().preALU2Queue.pop_front();
            }

            break;
        }
    }
}

void ALU3(){
    auto &inst = Program::instance().preALU3Queue.front();
    auto &instruction = Program::instance().getInstructions();
    auto &registers = Program::instance().getRegisters();
    auto &memory = Program::instance().getMemory();

    switch ((inst->category())) {
        case Instruction::Category::CAT2: {
            auto cat2 = (Cat2Instruction *)(inst.get());
            int rs1 = (int)cat2->bits.rs1.to_ulong();
            int rs2 = (int)cat2->bits.rs2.to_ulong();
            int result;

            if (OPCODES_2[cat2->bits.opcode.to_string()] == "and") {
                result = (int)(bitset<32>(registers[rs1]) & bitset<32>(registers[rs2])).to_ulong();
            }
            else if (OPCODES_2[cat2->bits.opcode.to_string()] == "or") {
                result = (int)(bitset<32>(registers[rs1]) | bitset<32>(registers[rs2])).to_ulong();
            }
            Program::instance().bufferedPostALU2Queue.emplace_back(cat2, result);
            Program::instance().preALU2Queue.pop_front();

            break;
        }
        case Instruction::Category::CAT3: {
            auto cat3 = (Cat3Instruction *)(inst.get());
            int rs1 = (int)cat3->bits.rs1.to_ulong();
            int rd = (int)cat3->bits.rd.to_ulong();
            int imm =  singedImm(cat3->bits.imm110.to_ulong(), cat3->bits.imm110.size());
            int result;

            if (OPCODES_3[cat3->bits.opcode.to_string()] == "andi") {
                result = (int)(bitset<32>(registers[rs1]) & bitset<32>(imm)).to_ulong();
            }
            else if (OPCODES_3[cat3->bits.opcode.to_string()] == "ori") {
                result = (int)(bitset<32>(registers[rs1]) | bitset<32>(imm)).to_ulong();
            }
            else if (OPCODES_3[cat3->bits.opcode.to_string()] == "slli") {
                result = (int)(bitset<32>(registers[rs1]) << imm).to_ulong();
            }
            else if (OPCODES_3[cat3->bits.opcode.to_string()] == "srai") {
                result = (int)(bitset<32>(registers[rs1]) >> imm).to_ulong();
            }
            Program::instance().bufferedPostALU3Queue.emplace_back(cat3, result);
            Program::instance().preALU3Queue.pop_front();
            break;
        }
    }
}

void MEM(){
    auto &inst = Program::instance().preMemQueue.front();
    auto &instruction = Program::instance().getInstructions();
    auto &registers = Program::instance().getRegisters();
    auto &memory = Program::instance().getMemory();

    switch ((inst.first->category())) {
        case Instruction::Category::CAT1: {

        }
        case Instruction::Category::CAT3: {
            auto cat3 = (Cat3Instruction*)inst.first.get();
            int rs1 = (int)cat3->bits.rs1.to_ulong();
            int rd = (int)cat3->bits.rd.to_ulong();
            int imm =  singedImm(cat3->bits.imm110.to_ulong(), cat3->bits.imm110.size());
            int location = inst.second;

            int memValue = (int)Program::instance().getValueAtMem(location).to_ulong();

            Program::instance().bufferedPostMemQueue.emplace_back(cat3, make_pair(location, memValue));

        }
    }
}

void WB(){

}

void runPipeLine(){
    int programCounter = 256;
    int pcAdd;

    while(runningPipeline){
        IF();
    }
}


// ================== main ==================

// runs the code.
int main(int argc, char* argv[]) {
    ifstream file(argv[1]);
    Program::instance().initialize();
    disassemble(file);
    writeDisassembly();
    auto& tet = Program::instance().getInstQueue();

    runningPipeline = true;
    runPipeLine();
    file.close();

    return 0;
}
