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
    FREE,
    QUEUED,
    DESTINATION,
    SOURCE,
    TARGET
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
    vector<RegisterRole>& getRegisterRoles() {return registerRoles; }
    vector<pair<int, bitset<32>>>& getMemory() { return memory; }
    int getCycle() {return cycle; }
    int getPC() {return programCounter; }
    queue<unique_ptr<Instruction>>& getInstQueue() { return inst_queue; }
    void initialize(){
        registers.assign(32, 0);
        registerRoles.assign(32, FREE);
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

    void setWaiting(const Instruction* instruction){
        IFwaiting = instruction;
    }
    void setExecuted(const Instruction* instruction){
        IFexecuted = instruction;
    }

    // These biffered ones are since I have the pipeline run in a way where eahc functional unit happens one after another
    // I need ot buffer adding the queues to ensure we dont go through everything in one cycle.
    deque<unique_ptr<Instruction>> bufferedPreIssueQueue;
    deque<unique_ptr<Instruction>> bufferedPreALU1Queue;
    deque<unique_ptr<Instruction>> bufferedPreALU2Queue;
    deque<unique_ptr<Instruction>> bufferedPreALU3Queue;

    deque<pair<unique_ptr<Instruction>, int>>    bufferedPreMemQueue;
    deque<pair<unique_ptr<Instruction>, int>>    bufferedPostMemQueue;
    deque<pair<unique_ptr<Instruction>, int>>    bufferedPostALU2Queue;
    deque<pair<unique_ptr<Instruction>, int>>    bufferedPostALU3Queue;

    deque<unique_ptr<Instruction>>    preIssueQueue;

    deque<unique_ptr<Instruction>>    preALU1Queue;
    deque<unique_ptr<Instruction>>    preALU2Queue;
    deque<unique_ptr<Instruction>>    preALU3Queue;

    deque<pair<unique_ptr<Instruction>, int>>    preMemQueue;
    deque<pair<unique_ptr<Instruction>, int>>    postMemQueue;
    deque<pair<unique_ptr<Instruction>, int>>    postALU2Queue;
    deque<pair<unique_ptr<Instruction>, int>>    postALU3Queue;

    const Instruction* IFwaiting  = nullptr;
    const Instruction* IFexecuted = nullptr;

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

// performs the logic for the instructions}

// ------------- Functional Units -------------

void IF(){
    auto &instruction = Program::instance().getInstructions();
    auto &registers = Program::instance().getRegisters();
    auto &registerState= Program::instance().getRegisterRoles();
    auto &memory = Program::instance().getMemory();

    int i = 0;
    while (i < 2) {
        int programCounter = Program::instance().getPC();
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
                        Program::instance().setExecuted(cat1);
                    }
                    else{
                        Program::instance().setWaiting(cat1);
                        i = 2;
                        break;
                    }
                }
                else{
                    if(Program::instance().preIssueQueue.size() + Program::instance().bufferedPreIssueQueue.size() < 4){
                        Program::instance().bufferedPreIssueQueue.push_back(unique_ptr<Instruction>(cat1));
                        Program::instance().modifyPC(4);
                        registerState[rs1] = (registerState[rs1] == FREE) ? QUEUED : registerState[rs1];
                        registerState[rs2] = (registerState[rs2] == FREE) ? QUEUED : registerState[rs1];
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
                int rs1 = (int)cat2->bits.rs1.to_ulong();
                int rs2 = (int)cat2->bits.rs2.to_ulong();
                int rd =  (int)cat2->bits.rd.to_ulong();
                if(Program::instance().preIssueQueue.size() + Program::instance().bufferedPreIssueQueue.size() < 4){
                    Program::instance().bufferedPreIssueQueue.push_back(unique_ptr<Instruction>(cat2));
                    Program::instance().modifyPC(4);
                    registerState[rs1] = (registerState[rs1] == FREE) ? QUEUED : registerState[rs1];
                    registerState[rs2] = (registerState[rs2] == FREE) ? QUEUED : registerState[rs1];
                    registerState[rd] = (registerState[rd] == FREE) ? QUEUED : registerState[rs1];
                }
                else{
                    i = 2;
                    break;
                }
                break;
            }
            case Instruction::Category::CAT3: {
                auto cat3 = (Cat3Instruction *) (instruction[(programCounter - 256) / 4]).get();
                int rs1 = (int)cat3->bits.rs1.to_ulong();
                int rd = (int)cat3->bits.rd.to_ulong();
                if(Program::instance().preIssueQueue.size() + Program::instance().bufferedPreIssueQueue.size() < 4){
                    Program::instance().bufferedPreIssueQueue.push_back(unique_ptr<Instruction>(cat3));
                    Program::instance().modifyPC(4);
                    registerState[rs1] = (registerState[rs1] == FREE) ? QUEUED : registerState[rs1];
                    registerState[rd] = (registerState[rd] == FREE) ? QUEUED : registerState[rs1];
                }
                else{
                    i = 2;
                    break;
                }
                break;
            }
            case Instruction::Category::CAT4: {

                auto cat4 = (Cat4Instruction *) (instruction[(programCounter - 256) / 4]).get();
                int rd =  (int)cat4->bits.rd.to_ulong();
                if(OPCODES_4[cat4->bits.opcode.to_string()] == "break"){
                    runningPipeline = false;
                    break;
                }
                if(OPCODES_4[cat4->bits.opcode.to_string()] == "jal"){
                    if (registerState[rd] == FREE) {
                        Program::instance().modifyPC(singedImm((cat4->bits.imm190 << 1).to_ulong(), (cat4->bits.imm190 << 1).size()));
                        Program::instance().instance().setExecuted(cat4);
                    }
                    else {
                        Program::instance().instance().setWaiting(cat4);
                    }
                }
                break;
            }
        }
        i++;
    }
}


void Isssue(){
    auto& registerState = Program::instance().getRegisterRoles();
    for (int i = 0; i < Program::instance().preIssueQueue.size(); ++i) {
        auto& inst = Program::instance().preIssueQueue[i];

        switch (inst->category()) {
        case Instruction::Category::CAT1: {
            auto* cat1 = static_cast<Cat1Instruction*>(inst.get());
            int rs1 = (int)cat1->bits.rs1.to_ulong();
            int rs2 = (int)cat1->bits.rs2.to_ulong();

            if (OPCODES_1[cat1->bits.opcode.to_string()] == "sw" &&
                // Only need to check RAW here
                (registerState[rs1] == FREE || registerState[rs1] == QUEUED || registerState[rs1] == DESTINATION) &&
                (registerState[rs2] == FREE || registerState[rs2] == QUEUED || registerState[rs2] == DESTINATION)) {


                // move ownership, erase by index, fix i
                Program::instance().bufferedPreALU1Queue.push_back(std::move(Program::instance().preIssueQueue[i]));
                Program::instance().preIssueQueue.erase(Program::instance().preIssueQueue.begin() + i);
                --i;

                registerState[rs1] = SOURCE;
                registerState[rs2] = SOURCE;
            }
            break;
        }

        case Instruction::Category::CAT2: {
            auto* cat2 = static_cast<Cat2Instruction*>(inst.get());
            int rs1 = (int)cat2->bits.rs1.to_ulong();
            int rs2 = (int)cat2->bits.rs2.to_ulong();
            int rd  = (int)cat2->bits.rd.to_ulong();

            if ((OPCODES_2[cat2->bits.opcode.to_string()] == "add" ||
                 OPCODES_2[cat2->bits.opcode.to_string()] == "sub") &&
                (registerState[rs1] == FREE || registerState[rs1] == QUEUED || registerState[rs1] == DESTINATION) &&
                (registerState[rs2] == FREE || registerState[rs2] == QUEUED || registerState[rs2] == DESTINATION) &&
                (registerState[rd]  == FREE || registerState[rd]  == QUEUED) || registerState[rd] == DESTINATION) {

                Program::instance().bufferedPreALU2Queue.push_back(std::move(Program::instance().preIssueQueue[i]));
                Program::instance().preIssueQueue.erase(Program::instance().preIssueQueue.begin() + i);
                --i;

                registerState[rs1] = SOURCE;
                registerState[rs2] = SOURCE;
                registerState[rd]  = DESTINATION;
            }
            else if (OPCODES_2[cat2->bits.opcode.to_string()] == "or" &&
                     (registerState[rs1] == FREE || registerState[rs1] == QUEUED) &&
                     (registerState[rs2] == FREE || registerState[rs2] == QUEUED) &&
                     (registerState[rd]  == FREE || registerState[rd]  == QUEUED)) {

                Program::instance().bufferedPreALU3Queue.push_back(std::move(Program::instance().preIssueQueue[i]));
                Program::instance().preIssueQueue.erase(Program::instance().preIssueQueue.begin() + i);
                --i;

                registerState[rs1] = SOURCE;
                registerState[rs2] = SOURCE;
                registerState[rd]  = DESTINATION;
            }
            break;
        }

        case Instruction::Category::CAT3: {
            auto* cat3 = static_cast<Cat3Instruction*>(inst.get());
            int rs1 = (int)cat3->bits.rs1.to_ulong();
            int rd  = (int)cat3->bits.rd.to_ulong();
            int imm = singedImm(cat3->bits.imm110.to_ulong(), cat3->bits.imm110.size());

            if (OPCODES_3[cat3->bits.opcode.to_string()] == "addi" &&
                (registerState[rs1] == FREE || registerState[rs1] == QUEUED) &&
                (registerState[rd]  == FREE || registerState[rd]  == QUEUED)) {

                Program::instance().bufferedPreALU2Queue.push_back(std::move(Program::instance().preIssueQueue[i]));
                Program::instance().preIssueQueue.erase(Program::instance().preIssueQueue.begin() + i);
                --i;

                registerState[rs1] = SOURCE;
                registerState[rd]  = DESTINATION;
            }
            else if ((OPCODES_3[cat3->bits.opcode.to_string()] == "andi" ||
                      OPCODES_3[cat3->bits.opcode.to_string()] == "ori"  ||
                      OPCODES_3[cat3->bits.opcode.to_string()] == "slli" ||
                      OPCODES_3[cat3->bits.opcode.to_string()] == "srai") &&
                     (registerState[rs1] == FREE || registerState[rs1] == QUEUED) &&
                     (registerState[rd]  == FREE || registerState[rd]  == QUEUED)) {

                Program::instance().bufferedPreALU3Queue.push_back(std::move(Program::instance().preIssueQueue[i]));
                Program::instance().preIssueQueue.erase(Program::instance().preIssueQueue.begin() + i);
                --i;

                registerState[rs1] = SOURCE;
                registerState[rd]  = DESTINATION;
            }
            else if (OPCODES_3[cat3->bits.opcode.to_string()] == "lw" &&
                     (registerState[rs1] == FREE || registerState[rs1] == QUEUED) &&
                     (registerState[rd]  == FREE || registerState[rd]  == QUEUED)) {

                Program::instance().bufferedPreALU1Queue.push_back(std::move(Program::instance().preIssueQueue[i]));
                Program::instance().preIssueQueue.erase(Program::instance().preIssueQueue.begin() + i);
                --i;

                registerState[rs1] = SOURCE;
                registerState[rd]  = DESTINATION;
            }
            break;
        }

        case Instruction::Category::CAT4:
            break;
        }
    }
}


void ALU1() {
    auto& prog = Program::instance();
    auto* inst = !prog.preALU1Queue.empty() ? prog.preALU1Queue.front().get() : nullptr;
    if (!inst) return;

    auto& registers = prog.getRegisters();

    switch (inst->category()) {
        case Instruction::Category::CAT1: {
            auto* cat1 = (Cat1Instruction*)inst;
            int rs1 = (int)cat1->bits.rs1.to_ulong();
            int rs2 = (int)cat1->bits.rs2.to_ulong();
            std::bitset<12> mem( cat1->bits.imm115.to_string() + cat1->bits.imm40.to_string() );

            if (OPCODES_1[cat1->bits.opcode.to_string()] == "sw") {
                int memToModify = (int)mem.to_ulong() + registers[rs2];
                // move owning unique_ptr from preALU1Queue into pair<unique_ptr<Instruction>, int>
                auto up = std::move(prog.preALU1Queue.front());
                prog.preALU1Queue.pop_front();
                prog.bufferedPreMemQueue.emplace_back(std::move(up), memToModify);
            }
            break;
        }

        case Instruction::Category::CAT3: {
            auto* cat3 = (Cat3Instruction*)inst;
            int rs1 = (int)cat3->bits.rs1.to_ulong();
            int imm  = singedImm(cat3->bits.imm110.to_ulong(), (int)cat3->bits.imm110.size());

            if (OPCODES_3[cat3->bits.opcode.to_string()] == "lw") {
                int memToRead = imm + registers[rs1];
                auto up = std::move(prog.preALU1Queue.front());
                prog.preALU1Queue.pop_front();
                prog.bufferedPreMemQueue.emplace_back(std::move(up), memToRead);
            }
            break;
        }
    }
}


void ALU2() {
    auto& prog = Program::instance();
    auto* inst = !prog.preALU2Queue.empty() ? prog.preALU2Queue.front().get() : nullptr;
    if (!inst) return;

    auto& registers = prog.getRegisters();

    switch (inst->category()) {
        case Instruction::Category::CAT2: {
            auto* cat2 = (Cat2Instruction*)inst;
            int rs1 = (int)cat2->bits.rs1.to_ulong();
            int rs2 = (int)cat2->bits.rs2.to_ulong();
            int result = 0;

            if (OPCODES_2[cat2->bits.opcode.to_string()] == "add") {
                result = registers[rs1] + registers[rs2];
            } else if (OPCODES_2[cat2->bits.opcode.to_string()] == "sub") {
                result = registers[rs1] - registers[rs2];
            }

            auto up = std::move(prog.preALU2Queue.front());
            prog.preALU2Queue.pop_front();
            prog.bufferedPostALU2Queue.emplace_back(std::move(up), result);
            break;
        }

        case Instruction::Category::CAT3: {
            auto* cat3 = (Cat3Instruction*)inst;
            int rs1 = (int)cat3->bits.rs1.to_ulong();
            int imm  = singedImm(cat3->bits.imm110.to_ulong(), (int)cat3->bits.imm110.size());

            if (OPCODES_3[cat3->bits.opcode.to_string()] == "addi") {
                int result = registers[rs1] + imm;
                auto up = std::move(prog.preALU2Queue.front());
                prog.preALU2Queue.pop_front();
                // Your code used post ALU3 for addi; keeping that routing:
                prog.bufferedPostALU3Queue.emplace_back(std::move(up), result);
            }
            break;
        }
    }
}


void ALU3() {
    auto& prog = Program::instance();
    auto* inst = !prog.preALU3Queue.empty() ? prog.preALU3Queue.front().get() : nullptr;
    if (!inst) return;

    auto& registers = prog.getRegisters();

    switch (inst->category()) {
        case Instruction::Category::CAT2: {
            auto* cat2 = (Cat2Instruction*)inst;
            int rs1 = (int)cat2->bits.rs1.to_ulong();
            int rs2 = (int)cat2->bits.rs2.to_ulong();
            int result = 0;

            if (OPCODES_2[cat2->bits.opcode.to_string()] == "and") {
                result = (int)(std::bitset<32>(registers[rs1]) & std::bitset<32>(registers[rs2])).to_ulong();
            } else if (OPCODES_2[cat2->bits.opcode.to_string()] == "or") {
                result = (int)(std::bitset<32>(registers[rs1]) | std::bitset<32>(registers[rs2])).to_ulong();
            }

            auto up = std::move(prog.preALU3Queue.front());
            prog.preALU3Queue.pop_front();
            // This should post to ALU3’s post queue, not ALU2
            prog.bufferedPostALU3Queue.emplace_back(std::move(up), result);
            break;
        }

        case Instruction::Category::CAT3: {
            auto* cat3 = (Cat3Instruction*)inst;
            int rs1 = (int)cat3->bits.rs1.to_ulong();
            int imm  = singedImm(cat3->bits.imm110.to_ulong(), (int)cat3->bits.imm110.size());
            int result = 0;

            if (OPCODES_3[cat3->bits.opcode.to_string()] == "andi") {
                result = (int)(std::bitset<32>(registers[rs1]) & std::bitset<32>(imm)).to_ulong();
            } else if (OPCODES_3[cat3->bits.opcode.to_string()] == "ori") {
                result = (int)(std::bitset<32>(registers[rs1]) | std::bitset<32>(imm)).to_ulong();
            } else if (OPCODES_3[cat3->bits.opcode.to_string()] == "slli") {
                result = (int)(std::bitset<32>(registers[rs1]) << imm).to_ulong();
            } else if (OPCODES_3[cat3->bits.opcode.to_string()] == "srai") {
                result = (int)(std::bitset<32>(registers[rs1]) >> imm).to_ulong();
            }

            auto up = std::move(prog.preALU3Queue.front());
            prog.preALU3Queue.pop_front();
            prog.bufferedPostALU3Queue.emplace_back(std::move(up), result);
            break;
        }
    }
}


void MEM() {
    auto& prog = Program::instance();
    auto* pairPtr = !prog.preMemQueue.empty() ? &prog.preMemQueue.front() : nullptr;
    if (!pairPtr) return;

    auto& registers = prog.getRegisters();

    switch (pairPtr->first->category()) {
        case Instruction::Category::CAT1: { // sw
            auto* cat1 = (Cat1Instruction*)pairPtr->first.get();
            int rs1 = (int)cat1->bits.rs1.to_ulong();
            int rs2 = (int)cat1->bits.rs2.to_ulong();

            int value = registers[rs1];
            int addr  = pairPtr->second;

            prog.modifyMemory(addr, value);
            prog.getRegisterRoles()[rs1] = FREE;
            prog.getRegisterRoles()[rs2] = FREE;

            // store completes here; retire this entry
            prog.preMemQueue.pop_front();
            break;
        }

        case Instruction::Category::CAT3: { // lw
            // We already have the computed address (second of pair)
            int addr = pairPtr->second;
            int memValue = (int)prog.getValueAtMem(addr).to_ulong();

            // Move the instruction unique_ptr along to Post-MEM with the loaded value
            auto up = std::move(pairPtr->first);
            prog.preMemQueue.pop_front();
            prog.bufferedPostMemQueue.emplace_back(std::move(up), memValue);
            break;
        }
    }
}


void WB() {
    auto& prog = Program::instance();
    auto& regs = prog.getRegisters();

    auto* postMem  = !prog.postMemQueue.empty()  ? &prog.postMemQueue.front()  : nullptr;
    auto* postALU2 = !prog.postALU2Queue.empty() ? &prog.postALU2Queue.front() : nullptr;
    auto* postALU3 = !prog.postALU3Queue.empty() ? &prog.postALU3Queue.front() : nullptr;

    // 1) Writeback from MEM (loads)
    if (postMem) {
        auto* cat3 = (Cat3Instruction*)postMem->first.get();
        int rd = (int)cat3->bits.rd.to_ulong();

        regs[rd] = postMem->second;
        prog.getRegisterRoles()[rd] = FREE;
        // rs1 (base) can be freed earlier; if not already, free here as needed.

        prog.postMemQueue.pop_front();
    }

    // 2) Writeback from ALU2
    if (postALU2) {
        switch (postALU2->first->category()) {
            case Instruction::Category::CAT2: {
                auto* cat2 = (Cat2Instruction*)postALU2->first.get();
                int rs1 = (int)cat2->bits.rs1.to_ulong();
                int rs2 = (int)cat2->bits.rs2.to_ulong();
                int rd  = (int)cat2->bits.rd.to_ulong();

                regs[rd] = postALU2->second;

                prog.getRegisterRoles()[rs1] = FREE;
                prog.getRegisterRoles()[rs2] = FREE;
                prog.getRegisterRoles()[rd]  = FREE;
                break;
            }
            case Instruction::Category::CAT3: {
                auto* cat3 = (Cat3Instruction*)postALU2->first.get();
                int rs1 = (int)cat3->bits.rs1.to_ulong();
                int rd  = (int)cat3->bits.rd.to_ulong();

                regs[rd] = postALU2->second;
                prog.getRegisterRoles()[rs1] = FREE;
                prog.getRegisterRoles()[rd]  = FREE;
                break;
            }
        }
        prog.postALU2Queue.pop_front();
    }

    // 3) Writeback from ALU3
    if (postALU3) {
        switch (postALU3->first->category()) {
            case Instruction::Category::CAT2: {
                auto* cat2 = (Cat2Instruction*)postALU3->first.get();
                int rs1 = (int)cat2->bits.rs1.to_ulong();
                int rs2 = (int)cat2->bits.rs2.to_ulong();
                int rd  = (int)cat2->bits.rd.to_ulong();

                regs[rd] = postALU3->second;

                prog.getRegisterRoles()[rs1] = FREE;
                prog.getRegisterRoles()[rs2] = FREE;
                prog.getRegisterRoles()[rd]  = FREE;
                break;
            }
            case Instruction::Category::CAT3: {
                auto* cat3 = (Cat3Instruction*)postALU3->first.get();
                int rs1 = (int)cat3->bits.rs1.to_ulong();
                int rd  = (int)cat3->bits.rd.to_ulong();

                regs[rd] = postALU3->second;

                prog.getRegisterRoles()[rs1] = FREE;
                prog.getRegisterRoles()[rd]  = FREE;
                break;
            }
        }
        prog.postALU3Queue.pop_front();
    }
}


// Helpers to ensure the buffered queues are add to the main queues.

template <typename T>
void appendQueue(deque<T>& dest, deque<T>& src) {
    while (!src.empty()) {
        dest.push_back(std::move(src.front()));
        src.pop_front();
    }
}

void commitBufferedQueues() {
    auto& prog = Program::instance();

    appendQueue(prog.preIssueQueue,  prog.bufferedPreIssueQueue);
    appendQueue(prog.preALU1Queue,   prog.bufferedPreALU1Queue);
    appendQueue(prog.preALU2Queue,   prog.bufferedPreALU2Queue);
    appendQueue(prog.preALU3Queue,   prog.bufferedPreALU3Queue);

    appendQueue(prog.preMemQueue,    prog.bufferedPreMemQueue);
    appendQueue(prog.postMemQueue,   prog.bufferedPostMemQueue);
    appendQueue(prog.postALU2Queue,  prog.bufferedPostALU2Queue);
    appendQueue(prog.postALU3Queue,  prog.bufferedPostALU3Queue);
}

static std::string formatInst(const Instruction* ins) {
    if (!ins) return "";
    switch (ins->category()) {
        case Instruction::Category::CAT1: {
            auto* i = (const Cat1Instruction*)ins;
            const std::string m = OPCODES_1[i->bits.opcode.to_string()];
            if (m == "sw") {
                std::bitset<12> imm(i->bits.imm115.to_string() + i->bits.imm40.to_string());
                return m + " x" + std::to_string(i->bits.rs1.to_ulong()) + ", "
                     + std::to_string(imm.to_ulong()) + "(x" + std::to_string(i->bits.rs2.to_ulong()) + ")";
            } else {
                std::bitset<12> imm(i->bits.imm115.to_string() + i->bits.imm40.to_string());
                return m + " x" + std::to_string(i->bits.rs1.to_ulong()) + ", "
                     + "x" + std::to_string(i->bits.rs2.to_ulong()) + ", "
                     + "#" + std::to_string(singedImm(imm.to_ulong(), 12));
            }
        }
        case Instruction::Category::CAT2: {
            auto* i = (const Cat2Instruction*)ins;
            const std::string m = OPCODES_2[i->bits.opcode.to_string()];
            return m + " x" + std::to_string(i->bits.rd.to_ulong()) + ", "
                     + "x" + std::to_string(i->bits.rs1.to_ulong()) + ", "
                     + "x" + std::to_string(i->bits.rs2.to_ulong());
        }
        case Instruction::Category::CAT3: {
            auto* i = (const Cat3Instruction*)ins;
            const std::string m = OPCODES_3[i->bits.opcode.to_string()];
            if (m == "lw") {
                return m + " x" + std::to_string(i->bits.rd.to_ulong()) + ", "
                     + std::to_string(singedImm(i->bits.imm110.to_ulong(), 12)) + "(x"
                     + std::to_string(i->bits.rs1.to_ulong()) + ")";
            } else {
                return m + " x" + std::to_string(i->bits.rd.to_ulong()) + ", "
                     + "x" + std::to_string(i->bits.rs1.to_ulong()) + ", "
                     + "#" + std::to_string(singedImm(i->bits.imm110.to_ulong(), 12));
            }
        }
        case Instruction::Category::CAT4: {
            auto* i = (const Cat4Instruction*)ins;
            const std::string m = OPCODES_4[i->bits.opcode.to_string()];
            if (m == "jal") {
                return m + " x" + std::to_string(i->bits.rd.to_ulong()) + ", "
                     + "#" + std::to_string(singedImm(i->bits.imm190.to_ulong(), 20));
            } else {
                return m; // break
            }
        }
    }
    return "";
}

template <typename Q>
static void printInstrQueue(const char* title, const Q& q) {
    std::cout << title << ":\n";
    if (q.empty()) return;
    int idx = 0;
    for (auto const& up : q) {
        const Instruction* ins = up.get();
        std::cout << "\tEntry " << idx++ << ": " << formatInst(ins) << "\n";
    }
}

template <typename Q>
static void printPairQueue(const char* title, const Q& q) {
    std::cout << title << ":\n";
    if (q.empty()) return;
    // Each entry is pair<unique_ptr<Instruction>, int>
    for (auto const& pr : q) {
        const Instruction* ins = pr.first.get();
        std::cout << "\t" << formatInst(ins) << "  (val=" << pr.second << ")\n";
    }
}

static void printRegisters() {
    auto& r = Program::instance().getRegisters();
    std::cout << "\nRegisters\n";
    for (int base = 0; base < 32; base += 8) {
        std::cout << "x" << (base < 10 ? "0" : "") << base << ":";
        for (int i = 0; i < 8; ++i) std::cout << "\t" << r[base + i];
        std::cout << "\n";
    }
}

static void printData() {
    auto& mem = Program::instance().getMemory();
    std::cout << "\nData\n";
    for (int i = 0; i < (int)mem.size(); ++i) {
        if (i % 8 == 0) std::cout << (memoryStart + i * 4) << ":";
        std::cout << "\t" << singedImm(mem[i].second.to_ulong(), mem[i].second.size());
        if (i % 8 == 7) std::cout << "\n";
    }
    if (!mem.empty() && mem.size() % 8 != 0) std::cout << "\n";
}

static void printCycleState() {
    std::cout << "--------------------\n";
    std::cout << "Cycle " << Program::instance().getCycle() << ":\n\n";

    // IF unit (best-effort — prints if you set these pointers elsewhere)
    std::cout << "IF Unit:\n";
    std::cout << "\tWaiting:  ";
    if (Program::instance().IFwaiting)  std::cout << formatInst(Program::instance().IFwaiting);
    std::cout << "\n\tExecuted: ";
    if (Program::instance().IFexecuted) std::cout << formatInst(Program::instance().IFexecuted);
    std::cout << "\n";

    // Queues
    printInstrQueue("Pre-Issue Queue", Program::instance().preIssueQueue);

    // ALU1 side
    printInstrQueue("Pre-ALU1 Queue", Program::instance().preALU1Queue);
    printPairQueue ("Pre-MEM Queue",  Program::instance().preMemQueue);
    printPairQueue ("Post-MEM Queue", Program::instance().postMemQueue);

    // ALU2 side
    printInstrQueue("Pre-ALU2 Queue", Program::instance().preALU2Queue);
    printPairQueue ("Post-ALU2 Queue",Program::instance().postALU2Queue);

    // ALU3 side
    printInstrQueue("Pre-ALU3 Queue", Program::instance().preALU3Queue);
    printPairQueue ("Post-ALU3 Queue",Program::instance().postALU3Queue);

    // Registers + Data
    printRegisters();
    printData();

    std::cout << std::flush;
}

void runPipeLine(){
    int programCounter = 256;
    int pcAdd;

    while(runningPipeline){
        IF();
        if (runningPipeline) {
            Isssue();
            ALU1();
            ALU2();
            ALU3();
            MEM();
            WB();
        }
        commitBufferedQueues();
        printCycleState();
        Program::instance().incrementCycle(1);
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
