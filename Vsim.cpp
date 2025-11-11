#include <iostream>
#include <ostream>
#include <fstream>
#include <string>
#include <vector>
#include <bitset>
#include <memory>
#include <unordered_map>
#include <queue>
#include <unordered_set>
/*
 * The code below was made before I saw we needed one to submit only 1 file
 * so there is an element of polymorphism here with Instruction class and
 * Program class. I liked how I had originally designed it so I stuck with it
 */

// Fix these #defines (remove the '=')
#define PREISSUEMAX 4
#define PREALU1MAX  2
#define PREALU2MAX  1
#define PREALU3MAX  1
#define PREMEMMAX   1
#define POSTMEMMAX  1
#define POSTALU2MAX 1
#define POSTALU3MAX 1



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

// Helpers for Issue hazard catchng

struct HazSnapshot {
    // who will write? who is being read?
    unordered_set<int> activeDests;   // regs that some in-flight instr will write
    unordered_set<int> activeSources; // regs that some in-flight instr will read
    bool olderStorePending = false;        // there exists a not-yet-issued older store
};

// returns rd for CAT2/CAT3 (non-lw/sw), or -1 if none
static int getDestReg(const Instruction* ins) {
    switch (ins->category()) {
        case Instruction::Category::CAT2: {
            auto* i = (const Cat2Instruction*)ins;
            return (int)i->bits.rd.to_ulong();
        }
        case Instruction::Category::CAT3: {
            auto* i = (const Cat3Instruction*)ins;
            const string m = OPCODES_3[i->bits.opcode.to_string()];
            if (m == "lw") return (int)i->bits.rd.to_ulong();
            // addi/andi/ori/slli/srai write rd
            return (int)i->bits.rd.to_ulong();
        }
        default: return -1;
    }
}

static void addSources(const Instruction* ins, unordered_set<int>& S) {
    switch (ins->category()) {
        case Instruction::Category::CAT1: { // sw uses rs1 (base) and rs2 (src)
            auto* i = (const Cat1Instruction*)ins;
            if (OPCODES_1[i->bits.opcode.to_string()] == "sw") {
                S.insert((int)i->bits.rs1.to_ulong());
                S.insert((int)i->bits.rs2.to_ulong());
            }
            break;
        }
        case Instruction::Category::CAT2: {
            auto* i = (const Cat2Instruction*)ins;
            S.insert((int)i->bits.rs1.to_ulong());
            S.insert((int)i->bits.rs2.to_ulong());
            break;
        }
        case Instruction::Category::CAT3: {
            auto* i = (const Cat3Instruction*)ins;
            const string m = OPCODES_3[i->bits.opcode.to_string()];
            // all CAT3 read rs1
            S.insert((int)i->bits.rs1.to_ulong());
            break;
        }
        default: break;
    }
}

static bool isStore(const Instruction* ins) {
    if (ins->category() != Instruction::Category::CAT1) return false;
    auto* i = (const Cat1Instruction*)ins;
    return OPCODES_1[i->bits.opcode.to_string()] == "sw";
}

static bool isLoad(const Instruction* ins) {
    if (ins->category() != Instruction::Category::CAT3) return false;
    auto* i = (const Cat3Instruction*)ins;
    return OPCODES_3[i->bits.opcode.to_string()] == "lw";
}

static bool isALU2Arith(const Instruction* ins) {
    if (ins->category() == Instruction::Category::CAT2) {
        auto* i = (const Cat2Instruction*)ins;
        auto m = OPCODES_2[i->bits.opcode.to_string()];
        return (m == "add" || m == "sub");
    }
    if (ins->category() == Instruction::Category::CAT3) {
        auto* i = (const Cat3Instruction*)ins;
        auto m = OPCODES_3[i->bits.opcode.to_string()];
        return (m == "addi");
    }
    return false;
}
static bool isALU3Logic(const Instruction* ins) {
    if (ins->category() == Instruction::Category::CAT2) {
        auto* i = (const Cat2Instruction*)ins;
        auto m = OPCODES_2[i->bits.opcode.to_string()];
        return (m == "and" || m == "or");
    }
    if (ins->category() == Instruction::Category::CAT3) {
        auto* i = (const Cat3Instruction*)ins;
        auto m = OPCODES_3[i->bits.opcode.to_string()];
        return (m == "andi" || m == "ori" || m == "slli" || m == "srai");
    }
    return false;
}

static HazSnapshot buildHazSnapshot() {
    auto& P = Program::instance();
    HazSnapshot H{};

    auto scanQ = [&](auto const& q) {
        for (auto const& up : q) {
            const Instruction* ins = up.get();
            int rd = getDestReg(ins);
            if (rd >= 0) H.activeDests.insert(rd);
            addSources(ins, H.activeSources);
            if (isStore(ins)) H.olderStorePending = true;
        }
    };
    auto scanPairQ = [&](auto const& q) {
        for (auto const& pr : q) {
            const Instruction* ins = pr.first.get();
            int rd = getDestReg(ins);
            if (rd >= 0) H.activeDests.insert(rd);
            addSources(ins, H.activeSources);
            if (isStore(ins)) H.olderStorePending = true;
        }
    };

    // everything except Pre-Issue (those are "not issued yet"; handled separately)
    scanQ(P.preALU1Queue);
    scanQ(P.preALU2Queue);
    scanQ(P.preALU3Queue);

    scanPairQ(P.preMemQueue);
    scanPairQ(P.postMemQueue);
    scanPairQ(P.postALU2Queue);
    scanPairQ(P.postALU3Queue);

    return H;
}


// ------------- Functional Units -------------

void IF() {
    auto &prog          = Program::instance();
    auto &instructions  = prog.getInstructions();
    auto &registers     = prog.getRegisters();
    auto &regState      = prog.getRegisterRoles();

    auto cloneInstruction = [](const Instruction* ins) -> std::unique_ptr<Instruction> {
        switch (ins->category()) {
            case Instruction::Category::CAT1:
                return std::make_unique<Cat1Instruction>(*static_cast<const Cat1Instruction*>(ins));
            case Instruction::Category::CAT2:
                return std::make_unique<Cat2Instruction>(*static_cast<const Cat2Instruction*>(ins));
            case Instruction::Category::CAT3:
                return std::make_unique<Cat3Instruction>(*static_cast<const Cat3Instruction*>(ins));
            case Instruction::Category::CAT4:
                return std::make_unique<Cat4Instruction>(*static_cast<const Cat4Instruction*>(ins));
            default:
                return nullptr;
        }
    };

    auto markQueued = [&](int reg) {
        if (reg >= 0 && reg < (int)regState.size() && regState[reg] == FREE) regState[reg] = QUEUED;
    };

    auto canFetchIntoPreIssue = [&](){
        return (int)prog.preIssueQueue.size() + (int)prog.bufferedPreIssueQueue.size() < PREISSUEMAX;
    };

    // ---------- CYCLE-START: clear last cycle's Executed snapshot ----------
    prog.setExecuted(nullptr);

    // ---------- If something is already Waiting, try to execute it (or stall) ----------
    if (prog.IFwaiting) {
        const Instruction* w = prog.IFwaiting;

        if (w->category() == Instruction::Category::CAT1) {
            auto* br = (Cat1Instruction*)w;
            int rs1 = (int)br->bits.rs1.to_ulong();
            int rs2 = (int)br->bits.rs2.to_ulong();

            std::string op = OPCODES_1[br->bits.opcode.to_string()];
            if (op == "beq" || op == "bne" || op == "blt") {
                // Branch executes in IF only when sources are FREE; otherwise stall.
                if (regState[rs1] != FREE || regState[rs2] != FREE) {
                    return; // keep it in Waiting; do NOT fetch
                }

                // Resolve branch
                std::bitset<12> imm(br->bits.imm115.to_string() + br->bits.imm40.to_string());
                int offset = (int)( (int32_t)( (uint32_t)imm.to_ulong() << 20 ) >> 20 ) << 1; // sign-extend then <<1

                bool take = false;
                if (op == "beq") take = (registers[rs1] == registers[rs2]);
                else if (op == "bne") take = (registers[rs1] != registers[rs2]);
                else if (op == "blt") take = (registers[rs1] <  registers[rs2]);

                if (take) prog.modifyPC(offset);
                else      prog.modifyPC(4);

                prog.setExecuted(w);
                prog.setWaiting(nullptr);     // remove from Waiting now
                // After executing a control flow, stop IF for this cycle.
                return;
            }
        }
        else if (w->category() == Instruction::Category::CAT4) {
            auto* j = (Cat4Instruction*)w;
            std::string op = OPCODES_4[j->bits.opcode.to_string()];
            if (op == "jal") {
                int rd = (int)j->bits.rd.to_ulong();
                if (regState[rd] != FREE) {
                    return; // stall until rd is FREE
                }
                int offset = singedImm((j->bits.imm190 << 1).to_ulong(), (int)(j->bits.imm190 << 1).size());
                prog.modifyPC(offset);
                prog.setExecuted(w);
                prog.setWaiting(nullptr);
                return;
            }
            // break should never be in Waiting; it ends in decode pass
        }

        // If Waiting holds a non-control (shouldn’t happen), just stop fetch to be safe.
        return;
    }

    // ---------- No Waiting: we may fetch/decode up to two, unless we hit control ----------
    int fetched = 0;
    bool firstWasBranch = false;

    auto fetchOne = [&]() -> Instruction* {
        int pc  = prog.getPC();
        int idx = (pc - 256) / 4;
        if (idx < 0 || idx >= (int)instructions.size()) return nullptr;
        return instructions[idx].get();
    };

    while (fetched < 2) {
        if (!canFetchIntoPreIssue()) break;

        Instruction* ins = fetchOne();
        if (!ins) break;

        switch (ins->category()) {
            case Instruction::Category::CAT1: {
                auto* c1 = (Cat1Instruction*)ins;
                std::string op = OPCODES_1[c1->bits.opcode.to_string()];
                bool is_branch = (op == "beq" || op == "bne" || op == "blt");

                if (is_branch) {
                    // Branch fetched now.
                    if (fetched == 0) {
                        // Branch is FIRST in the pair -> park in Waiting and DISCARD the second fetch this cycle.
                        prog.setWaiting(c1);
                        // Do NOT advance PC to the next-next instruction; branch’s outcome decides next PC.
                        // We simply stop fetching now; the "next" instruction will be re-fetched later.
                        fetched = 2;        // stop loop
                        firstWasBranch = true;
                    } else {
                        // Branch is SECOND -> first instr already decoded; park branch, stop.
                        prog.setWaiting(c1);
                        // Same as above: we stop; next fetch will occur after branch outcome.
                        fetched = 2;
                    }
                    break;
                }

                // Non-branch CAT1 (e.g., sw)
                int rs1 = (int)c1->bits.rs1.to_ulong();
                int rs2 = (int)c1->bits.rs2.to_ulong();
                prog.bufferedPreIssueQueue.emplace_back(cloneInstruction(c1));
                prog.modifyPC(4);
                markQueued(rs1);
                markQueued(rs2);
                ++fetched;
                break;
            }

            case Instruction::Category::CAT2: {
                // Arithmetic/logical (R-type)
                auto* c2 = (Cat2Instruction*)ins;
                int rs1 = (int)c2->bits.rs1.to_ulong();
                int rs2 = (int)c2->bits.rs2.to_ulong();
                int rd  = (int)c2->bits.rd.to_ulong();
                prog.bufferedPreIssueQueue.emplace_back(cloneInstruction(c2));
                prog.modifyPC(4);
                markQueued(rs1); markQueued(rs2); markQueued(rd);
                ++fetched;
                break;
            }

            case Instruction::Category::CAT3: {
                // addi/andi/ori/slli/srai/lw
                auto* c3 = (Cat3Instruction*)ins;
                int rs1 = (int)c3->bits.rs1.to_ulong();
                int rd  = (int)c3->bits.rd.to_ulong();
                prog.bufferedPreIssueQueue.emplace_back(cloneInstruction(c3));
                prog.modifyPC(4);
                markQueued(rs1); markQueued(rd);
                ++fetched;
                break;
            }

            case Instruction::Category::CAT4: {
                auto* c4 = (Cat4Instruction*)ins;
                std::string op = OPCODES_4[c4->bits.opcode.to_string()];

                if (op == "break") {
                    runningPipeline = false;
                    fetched = 2;
                    break;
                }

                if (op == "jal") {
                    // Park in Waiting first (per pairing rule)
                    prog.setWaiting(c4);

                    // If JAL is FIRST fetched this cycle, we must discard the second fetch and stop.
                    if (fetched == 0) {
                        fetched = 2;            // stop fetch loop
                        break;                  // execute next cycle when ready
                    }

                    // JAL is SECOND fetched this cycle.
                    // If rd is FREE, execute it immediately in this same cycle.
                    int rd = (int)c4->bits.rd.to_ulong();
                    if (regState[rd] == FREE) {
                        int offset = singedImm((c4->bits.imm190 << 1).to_ulong(),
                                               (int)(c4->bits.imm190 << 1).size());
                        prog.modifyPC(offset);
                        prog.setExecuted(c4);
                        prog.setWaiting(nullptr);
                    }
                    fetched = 2;    // stop fetch loop either way
                    break;
                }

                // Unknown CAT4
                fetched = 2;
                break;
            }

        }
    }
}




void Issue() {
    auto& P = Program::instance();

    // Structural capacity snapshot (beginning-of-cycle)
    bool alu1Free = (P.preALU1Queue.size() < PREALU1MAX);
    bool alu2Free = (P.preALU2Queue.size() < PREALU2MAX);
    bool alu3Free = (P.preALU3Queue.size() < PREALU3MAX);

    int issuedTotal = 0;
    bool usedALU1 = false, usedALU2 = false, usedALU3 = false;

    // In-flight hazard snapshot (everything except preIssue)
    HazSnapshot H = buildHazSnapshot();

    // Track same-cycle hazards among instructions we issue this cycle
    std::unordered_set<int> thisCycleDests, thisCycleSources;

    // ─────────────────────────────────────────────────────────────────────────
    // NEW: gate to block issuing past OLDER Pre-Issue entries with hazards
    auto hazardsWithOlderPreIssue = [&](int myIndex, int myRD,
                                        const std::unordered_set<int>& myRS) {
        for (int j = 0; j < myIndex; ++j) {
            const Instruction* older = P.preIssueQueue[j].get();
            int olderRD = getDestReg(older);
            std::unordered_set<int> olderRS;
            addSources(older, olderRS);

            // RAW: my rs vs older rd
            for (int r : myRS) if (olderRD >= 0 && r == olderRD) return true;
            // WAW: my rd vs older rd
            if (myRD >= 0 && olderRD >= 0 && myRD == olderRD) return true;
            // WAR: my rd vs older rs
            if (myRD >= 0 && olderRS.count(myRD)) return true;
        }
        return false;
    };
    // ─────────────────────────────────────────────────────────────────────────

    // Scan Pre-Issue entries 0..3 (by index), max 3 issues total, ≤1/FU
    for (int i = 0; i < (int)P.preIssueQueue.size() && issuedTotal < 3; ++i) {
        Instruction* ins = P.preIssueQueue[i].get();

        // Classify FU target
        bool toALU1 = (isLoad(ins) || isStore(ins));
        bool toALU2 = isALU2Arith(ins);
        bool toALU3 = isALU3Logic(ins);

        // Structural guard (beginning-of-cycle capacity + one per FU per cycle)
        if (toALU1 && (!alu1Free || usedALU1)) continue;
        if (toALU2 && (!alu2Free || usedALU2)) continue;
        if (toALU3 && (!alu3Free || usedALU3)) continue;

        // MEM ordering special rules
        if (isStore(ins)) {
            // stores issue in order: any older store still in Pre-Issue blocks me
            bool olderStore = false;
            for (int j = 0; j < i; ++j)
                if (isStore(P.preIssueQueue[j].get())) { olderStore = true; break; }
            if (olderStore) continue;
        }
        if (isLoad(ins)) {
            // loads wait until all older stores (anywhere) are issued
            bool olderStoreInPreIssue = false;
            for (int j = 0; j < i; ++j)
                if (isStore(P.preIssueQueue[j].get())) { olderStoreInPreIssue = true; break; }
            if (olderStoreInPreIssue || H.olderStorePending) continue;
        }

        // Compute this instruction's sources/dest
        std::unordered_set<int> rs;
        addSources(ins, rs);
        int rd = getDestReg(ins);

        // ─────────────────────────────────────────────────────────────────────
        // NEW: DO NOT bypass older Pre-Issue entries if there’s any reg hazard
        if (hazardsWithOlderPreIssue(i, rd, rs)) continue;
        // ─────────────────────────────────────────────────────────────────────

        // In-flight (already issued) hazards + same-cycle hazards
        auto hasRAW = [&]{
            for (int r : rs) if (H.activeDests.count(r) || thisCycleDests.count(r)) return true;
            return false;
        };
        auto hasWAW = [&]{
            if (rd < 0) return false;
            return H.activeDests.count(rd) || thisCycleDests.count(rd);
        };
        auto hasWAR = [&]{
            if (rd < 0) return false;
            return H.activeSources.count(rd) || thisCycleSources.count(rd);
        };
        if (hasRAW() || hasWAW() || hasWAR()) continue;

        // Passed all checks → issue to FU buffer
        if (toALU1) {
            P.bufferedPreALU1Queue.push_back(std::move(P.preIssueQueue[i]));
            usedALU1 = true;
        } else if (toALU2) {
            P.bufferedPreALU2Queue.push_back(std::move(P.preIssueQueue[i]));
            usedALU2 = true;
        } else if (toALU3) {
            P.bufferedPreALU3Queue.push_back(std::move(P.preIssueQueue[i]));
            usedALU3 = true;
        } else {
            continue; // non-issuable type (shouldn't happen)
        }

        // Remove from Pre-Issue; keep index in sync
        P.preIssueQueue.erase(P.preIssueQueue.begin() + i);
        --i;

        // Book same-cycle hazards
        for (int r : rs) thisCycleSources.insert(r);
        if (rd >= 0) thisCycleDests.insert(rd);

        ++issuedTotal;
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
            bitset<12> mem( cat1->bits.imm115.to_string() + cat1->bits.imm40.to_string() );

            if (OPCODES_1[cat1->bits.opcode.to_string()] == "sw") {
                int memToModify = (int)mem.to_ulong() + registers[rs2];
                // move owning unique_ptr from preALU1Queue into pair<unique_ptr<Instruction>, int>
                auto up = move(prog.preALU1Queue.front());
                prog.preALU1Queue.pop_front();
                prog.bufferedPreMemQueue.emplace_back(move(up), memToModify);
            }
            break;
        }

        case Instruction::Category::CAT3: {
            auto* cat3 = (Cat3Instruction*)inst;
            int rs1 = (int)cat3->bits.rs1.to_ulong();
            int imm  = singedImm(cat3->bits.imm110.to_ulong(), (int)cat3->bits.imm110.size());

            if (OPCODES_3[cat3->bits.opcode.to_string()] == "lw") {
                int memToRead = imm + registers[rs1];
                auto up = move(prog.preALU1Queue.front());
                prog.preALU1Queue.pop_front();
                prog.bufferedPreMemQueue.emplace_back(move(up), memToRead);
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

            auto up = move(prog.preALU2Queue.front());
            prog.preALU2Queue.pop_front();
            prog.bufferedPostALU2Queue.emplace_back(move(up), result);
            break;
        }

        case Instruction::Category::CAT3: {
            auto* cat3 = (Cat3Instruction*)inst;
            int rs1 = (int)cat3->bits.rs1.to_ulong();
            int imm  = singedImm(cat3->bits.imm110.to_ulong(), (int)cat3->bits.imm110.size());

            if (OPCODES_3[cat3->bits.opcode.to_string()] == "addi") {
                int result = registers[rs1] + imm;
                auto up = move(prog.preALU2Queue.front());
                prog.preALU2Queue.pop_front();
                // Your code used post ALU3 for addi; keeping that routing:
                prog.bufferedPostALU2Queue.emplace_back(move(up), result);
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
                result = (int)(bitset<32>(registers[rs1]) & bitset<32>(registers[rs2])).to_ulong();
            } else if (OPCODES_2[cat2->bits.opcode.to_string()] == "or") {
                result = (int)(bitset<32>(registers[rs1]) | bitset<32>(registers[rs2])).to_ulong();
            }

            auto up = move(prog.preALU3Queue.front());
            prog.preALU3Queue.pop_front();
            // This should post to ALU3’s post queue, not ALU2
            prog.bufferedPostALU3Queue.emplace_back(move(up), result);
            break;
        }

        case Instruction::Category::CAT3: {
            auto* cat3 = (Cat3Instruction*)inst;
            int rs1 = (int)cat3->bits.rs1.to_ulong();
            int imm  = singedImm(cat3->bits.imm110.to_ulong(), (int)cat3->bits.imm110.size());
            int result = 0;

            if (OPCODES_3[cat3->bits.opcode.to_string()] == "andi") {
                result = (int)(bitset<32>(registers[rs1]) & bitset<32>(imm)).to_ulong();
            } else if (OPCODES_3[cat3->bits.opcode.to_string()] == "ori") {
                result = (int)(bitset<32>(registers[rs1]) | bitset<32>(imm)).to_ulong();
            } else if (OPCODES_3[cat3->bits.opcode.to_string()] == "slli") {
                result = (int)(bitset<32>(registers[rs1]) << imm).to_ulong();
            } else if (OPCODES_3[cat3->bits.opcode.to_string()] == "srai") {
                result = (int)(bitset<32>(registers[rs1]) >> imm).to_ulong();
            }

            auto up = move(prog.preALU3Queue.front());
            prog.preALU3Queue.pop_front();
            prog.bufferedPostALU3Queue.emplace_back(move(up), result);
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
            auto up = move(pairPtr->first);
            prog.preMemQueue.pop_front();
            prog.bufferedPostMemQueue.emplace_back(move(up), memValue);
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
        dest.push_back(move(src.front()));
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

static string formatInst(const Instruction* ins) {
    if (!ins) return "";
    switch (ins->category()) {
        case Instruction::Category::CAT1: {
            auto* i = (const Cat1Instruction*)ins;
            const string m = OPCODES_1[i->bits.opcode.to_string()];
            if (m == "sw") {
                bitset<12> imm(i->bits.imm115.to_string() + i->bits.imm40.to_string());
                return m + " x" + to_string(i->bits.rs1.to_ulong()) + ", "
                     + to_string(imm.to_ulong()) + "(x" + to_string(i->bits.rs2.to_ulong()) + ")";
            } else {
                bitset<12> imm(i->bits.imm115.to_string() + i->bits.imm40.to_string());
                return m + " x" + to_string(i->bits.rs1.to_ulong()) + ", "
                     + "x" + to_string(i->bits.rs2.to_ulong()) + ", "
                     + "#" + to_string(singedImm(imm.to_ulong(), 12));
            }
        }
        case Instruction::Category::CAT2: {
            auto* i = (const Cat2Instruction*)ins;
            const string m = OPCODES_2[i->bits.opcode.to_string()];
            return m + " x" + to_string(i->bits.rd.to_ulong()) + ", "
                     + "x" + to_string(i->bits.rs1.to_ulong()) + ", "
                     + "x" + to_string(i->bits.rs2.to_ulong());
        }
        case Instruction::Category::CAT3: {
            auto* i = (const Cat3Instruction*)ins;
            const string m = OPCODES_3[i->bits.opcode.to_string()];
            if (m == "lw") {
                return m + " x" + to_string(i->bits.rd.to_ulong()) + ", "
                     + to_string(singedImm(i->bits.imm110.to_ulong(), 12)) + "(x"
                     + to_string(i->bits.rs1.to_ulong()) + ")";
            } else {
                return m + " x" + to_string(i->bits.rd.to_ulong()) + ", "
                     + "x" + to_string(i->bits.rs1.to_ulong()) + ", "
                     + "#" + to_string(singedImm(i->bits.imm110.to_ulong(), 12));
            }
        }
        case Instruction::Category::CAT4: {
            auto* i = (const Cat4Instruction*)ins;
            const string m = OPCODES_4[i->bits.opcode.to_string()];
            if (m == "jal") {
                return m + " x" + to_string(i->bits.rd.to_ulong()) + ", "
                     + "#" + to_string(singedImm(i->bits.imm190.to_ulong(), 20));
            } else {
                return m; // break
            }
        }
    }
    return "";
}


static std::string bracket(const std::string& s) {
    return s.empty() ? std::string{} : "[" + s + "]";
}

static std::string fmtBracket(const Instruction* ins) {
    return bracket(formatInst(ins));
}

static std::ofstream simout;

// IF unit
static void printIFUnit() {
    simout << "IF Unit:\n";
    simout << "\tWaiting: "  << fmtBracket(Program::instance().IFwaiting)  << "\n";
    simout << "\tExecuted: " << fmtBracket(Program::instance().IFexecuted) << "\n";
}

// Fixed-count entries (always print Entry lines) — used for Pre-Issue and Pre-ALU1
template <typename Q>
static void printFixedEntries(const char* title, const Q& q, int capacity) {
    simout << title << ":\n";
    int idx = 0;
    for (auto const& up : q) {
        if (idx >= capacity) break;
        simout << "\tEntry " << idx++ << ": " << fmtBracket(up.get()) << "\n";
    }
    while (idx < capacity) {
        simout << "\tEntry " << idx++ << ":\n";
    }
}

// When empty: print ONLY the header line (no "Entry 0:").
// When non-empty: print up to `capacity` Entry lines.
// Use this for Pre-ALU2 and Pre-ALU3 to match the target diff.
template <typename Q>
static void printMaybeEntries(const char* title, const Q& q, int capacity) {
    if (q.empty()) {
        simout << title << ":\n";
        return;
    }
    // non-empty → show entries (capacity lines max)
    simout << title << ":\n";
    int idx = 0;
    for (auto const& up : q) {
        if (idx >= capacity) break;
        simout << "\tEntry " << idx++ << ": " << fmtBracket(up.get()) << "\n";
    }
}

// Single-line queues that store pair<unique_ptr<Instruction>,T>
// IMPORTANT: no trailing space after colon if empty
template <typename Q>
static void printSingleLineQueuePair(const char* title, const Q& q) {
    simout << title << ":";
    if (!q.empty()) {
        simout << " " << fmtBracket(q.front().first.get());
    }
    simout << "\n";
}

// Single-line queues that store unique_ptr<Instruction>
template <typename Q>
static void printSingleLineQueue(const char* title, const Q& q) {
    simout << title << ":";
    if (!q.empty()) {
        simout << " " << fmtBracket(q.front().get());
    }
    simout << "\n";
}

static void printRegistersStream() {
    auto& r = Program::instance().getRegisters();
    simout << "\nRegisters\n";
    for (int base = 0; base < 32; base += 8) {
        simout << "x" << (base < 10 ? "0" : "") << base << ":";
        for (int i = 0; i < 8; ++i) simout << "\t" << r[base + i];
        simout << "\n";
    }
}

static void printDataStream() {
    auto& mem = Program::instance().getMemory();
    simout << "\nData\n";
    for (int i = 0; i < (int)mem.size(); ++i) {
        if (i % 8 == 0) simout << (memoryStart + i * 4) << ":";
        simout << "\t" << singedImm(mem[i].second.to_ulong(), mem[i].second.size());
        if (i % 8 == 7) simout << "\n";
    }
    if (!mem.empty() && mem.size() % 8 != 0) simout << "\n";
}


static void printCycleStateStream() {
    simout << "--------------------\n";
    simout << "Cycle " << Program::instance().getCycle() << ":\n\n";

    printIFUnit();

    printFixedEntries("Pre-Issue Queue", Program::instance().preIssueQueue, PREISSUEMAX);

    printFixedEntries("Pre-ALU1 Queue", Program::instance().preALU1Queue, PREALU1MAX);
    printSingleLineQueuePair("Pre-MEM Queue",  Program::instance().preMemQueue);
    printSingleLineQueuePair("Post-MEM Queue", Program::instance().postMemQueue);

    // ⬇️ use MaybeEntries so empty ALU2/ALU3 show only the header line
    printMaybeEntries("Pre-ALU2 Queue", Program::instance().preALU2Queue, PREALU2MAX);
    printSingleLineQueuePair("Post-ALU2 Queue", Program::instance().postALU2Queue);

    printMaybeEntries("Pre-ALU3 Queue", Program::instance().preALU3Queue, PREALU3MAX);
    printSingleLineQueuePair("Post-ALU3 Queue", Program::instance().postALU3Queue);

    printRegistersStream();
    printDataStream();
}




void runPipeLine(){
    int programCounter = 256;
    int pcAdd;

    while(runningPipeline){
        IF();
        if (runningPipeline) {
            Issue();
            ALU1();
            ALU2();
            ALU3();
            MEM();
            WB();
        }
        commitBufferedQueues();
        printCycleStateStream();
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
    simout.open("simulation.txt"); // <-- write here
    runPipeLine();
    file.close();

    return 0;
}