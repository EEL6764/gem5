#include "cpu/o3/probe/inst_csv_trace.hh"

#include <iomanip>
#include <sstream>

#include "base/output.hh"
#include "base/trace.hh"
#include "cpu/o3/dyn_inst.hh"
#include "cpu/static_inst.hh"
#include "debug/InstCsvTrace.hh"
#include "sim/core.hh"
#include "sim/cur_tick.hh"
#include "sim/sim_exit.hh"

namespace gem5
{

namespace o3
{

InstCsvTrace::InstCsvTrace(const InstCsvTraceParams &params)
    : ProbeListenerObject(params),
      traceFileName(params.trace_file),
      traceFetchEnabled(params.trace_fetch),
      traceMemEnabled(params.trace_mem),
      startAfterInst(params.start_after_inst),
      stopAfterInst(params.stop_after_inst),
      flushInterval(params.flush_interval),
      committedInsts(0),
      tracedInsts(0),
      tracingActive(params.start_after_inst == 0),
      firstRecord(true)
{

    traceFile.open(simout.resolve(traceFileName));
    if (!traceFile.is_open()) {
        fatal("InstCsvTrace: Could not open output file %s",
              traceFileName.c_str());
    }

    traceFile << "seq_num,pc,disasm,op_class,thread_id,fetch_tick,commit_tick,total_latency,is_load,is_store,is_branch,is_call,is_return,is_integer,is_floating,eff_addr,mem_size\n";

    DPRINTF(InstCsvTrace, "InstCsvTrace initialized, output file: %s\n",
            traceFileName.c_str());

    gem5::registerExitCallback([this]() { closeFile(); });
}

InstCsvTrace::~InstCsvTrace()
{
    closeFile();
}

void
InstCsvTrace::closeFile()
{
    if (traceFile.is_open()) {
        traceFile.close();
        DPRINTF(InstCsvTrace, "InstCsvTrace closed, traced %lu of %lu instructions\n",
                tracedInsts, committedInsts);
    }
}

void
InstCsvTrace::regProbeListeners()
{
    typedef ProbeListenerArg<InstCsvTrace, DynInstConstPtr> DynInstListener;

    // Listen to commit events
    listeners.push_back(new DynInstListener(this, "Commit",
                                            &InstCsvTrace::traceCommit));

    // Listen to fetch events if enabled
    if (traceFetchEnabled) {
        listeners.push_back(new DynInstListener(this, "Fetch",
                                                &InstCsvTrace::traceFetch));
    }

    DPRINTF(InstCsvTrace, "Registered probe listeners\n");
}

void
InstCsvTrace::traceFetch(const DynInstConstPtr& dynInst)
{
    if (!dynInst)
        return;

    InstTimingInfo& timing = timingMap[dynInst->seqNum];
    timing.fetchTick = curTick();

    DPRINTF(InstCsvTrace, "Fetch: seqNum=%lu, PC=0x%x, tick=%lu\n",
            dynInst->seqNum, dynInst->pcState().instAddr(), curTick());
}

void
InstCsvTrace::traceCommit(const DynInstConstPtr& dynInst)
{
    if (!dynInst)
        return;

    committedInsts++;

    if (!tracingActive && committedInsts >= startAfterInst) {
        tracingActive = true;
        DPRINTF(InstCsvTrace, "Starting trace at instruction %lu\n",
                committedInsts);
    }

    if (stopAfterInst > 0 && tracedInsts >= stopAfterInst) {
        tracingActive = false;
    }

    if (!tracingActive) {
        timingMap.erase(dynInst->seqNum);
        return;
    }

    InstTimingInfo timing;
    auto it = timingMap.find(dynInst->seqNum);
    if (it != timingMap.end()) {
        timing = it->second;
        timingMap.erase(it);
    }
    timing.commitTick = curTick();

    writeCsvRecord(dynInst, timing);
    tracedInsts++;

    if (flushInterval > 0 && (tracedInsts % flushInterval) == 0) {
        flushOutput();
    }

    DPRINTF(InstCsvTrace, "Commit: seqNum=%lu, PC=0x%x, tick=%lu\n",
            dynInst->seqNum, dynInst->pcState().instAddr(), curTick());
}

void
InstCsvTrace::writeCsvRecord(const DynInstConstPtr& dynInst,
                               const InstTimingInfo& timing)
{
    if (!traceFile.is_open())
        return;

    Addr pc = dynInst->pcState().instAddr();
    std::string disasm = dynInst->staticInst->disassemble(pc);
    std::string opClassName = enums::OpClassStrings[dynInst->opClass()];

    Tick latency = 0;
    if (traceFetchEnabled && timing.fetchTick > 0) {
        latency = timing.commitTick - timing.fetchTick;
    }

    traceFile << dynInst->seqNum << ","
              << "0x" << std::hex << pc << std::dec << ","
              << "\"" << escapeCsv(disasm) << "\","
              << opClassName << ","
              << dynInst->threadNumber << ","
              << timing.fetchTick << ","
              << timing.commitTick << ","
              << latency << ","
              << (dynInst->isLoad() ? 1 : 0) << ","
              << (dynInst->isStore() ? 1 : 0) << ","
              << (dynInst->isControl() ? 1 : 0) << ","
              << (dynInst->isCall() ? 1 : 0) << ","
              << (dynInst->isReturn() ? 1 : 0) << ","
              << (dynInst->isInteger() ? 1 : 0) << ","
              << (dynInst->isFloating() ? 1 : 0) << ",";

    if (dynInst->isLoad() || dynInst->isStore()) {
        traceFile << "0x" << std::hex << dynInst->effAddr << std::dec << ","
                  << dynInst->effSize;
    } else {
        traceFile << ",";
    }
    traceFile << "\n";
}

std::string
InstCsvTrace::escapeCsv(const std::string& str)
{
    std::ostringstream escaped;
    for (char c : str) {
        if (c == '"') {
            escaped << "\"\"";  // Double quotes to escape in CSV
        } else if (c == '\n' || c == '\r') {
            escaped << " ";  // Replace newlines with space
        } else {
            escaped << c;
        }
    }
    return escaped.str();
}

void
InstCsvTrace::flushOutput()
{
    if (traceFile.is_open()) {
        traceFile.flush();
    }
}

}
}
