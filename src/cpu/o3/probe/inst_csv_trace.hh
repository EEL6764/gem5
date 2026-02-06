/*
 * Copyright (c) 2024
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 * This file implements a probe listener that outputs CSV records for each
 * committed instruction in the O3 pipeline. It captures detailed timing
 * information including fetch, decode, rename, issue, execute, and commit
 * timestamps.
 */

#ifndef __CPU_O3_PROBE_INST_JSON_TRACE_HH__
#define __CPU_O3_PROBE_INST_JSON_TRACE_HH__

#include <fstream>
#include <string>
#include <unordered_map>

#include "cpu/inst_seq.hh"
#include "cpu/o3/dyn_inst_ptr.hh"
#include "params/InstCsvTrace.hh"
#include "sim/probe/probe.hh"
#include "sim/sim_exit.hh"

namespace gem5
{

namespace o3
{

/**
 * InstJsonTrace is a probe listener that outputs JSON records for each
 * instruction that is committed by the O3 CPU.
 *
 * The JSON output contains:
 * - Instruction sequence number
 * - PC (program counter) address
 * - Disassembled instruction
 * - Instruction type/opclass
 * - Timing information:
 *   - Fetch tick
 *   - Commit tick
 * - Memory access info (for loads/stores):
 *   - Effective address
 *   - Access size
 *   - Whether it's a load or store
 * - Thread ID
 */
class InstCsvTrace : public ProbeListenerObject
{
  public:
    InstCsvTrace(const InstCsvTraceParams &params);
    ~InstCsvTrace();
    /** Register the probe listeners */
    void regProbeListeners() override;

    std::string name() const override
    {
        return ProbeListenerObject::name() + ".instCsvTrace";
    }

  private:
    /**
     * Struct to hold timing information collected during instruction
     * execution. This is populated as the instruction flows through
     * the pipeline stages.
     */
    struct InstTimingInfo
    {
        Tick fetchTick = 0;      ///< When instruction was fetched
        Tick decodeTick = 0;     ///< When instruction was decoded
        Tick renameTick = 0;     ///< When instruction was renamed
        Tick dispatchTick = 0;   ///< When instruction was dispatched to IQ
        Tick issueTick = 0;      ///< When instruction was issued
        Tick executeTick = 0;    ///< When instruction started execution
        Tick completeTick = 0;   ///< When instruction completed execution
        Tick commitTick = 0;     ///< When instruction was committed
    };

    /** Called when an instruction is fetched */
    void traceFetch(const DynInstConstPtr& dynInst);

    /** Called when an instruction is committed */
    void traceCommit(const DynInstConstPtr& dynInst);

    /** Write a CSV record for a committed instruction */
    void writeCsvRecord(const DynInstConstPtr& dynInst,
                         const InstTimingInfo& timing);

    /** Escape special characters in a string for CSV output */
    std::string escapeCsv(const std::string& str);

    /** Flush the output stream */
    void flushOutput();

    /** Close the output file and finalize CSV */
    void closeFile();

    /** Output file stream */
    std::ofstream traceFile;

    /** File name for output */
    std::string traceFileName;

    /** Whether to trace fetch timing */
    bool traceFetchEnabled;

    /** Whether to trace memory access info */
    bool traceMemEnabled;

    /** Start tracing after this many instructions */
    uint64_t startAfterInst;

    /** Stop tracing after this many instructions (0 = unlimited) */
    uint64_t stopAfterInst;

    /** Flush interval (instructions) */
    uint64_t flushInterval;

    /** Counter for committed instructions */
    uint64_t committedInsts;

    /** Counter for traced instructions */
    uint64_t tracedInsts;

    /** Whether tracing is currently active */
    bool tracingActive;

    /** Whether the first record has been written (for JSON comma handling) */
    bool firstRecord;

    /** Map from sequence number to timing info for in-flight instructions */
    std::unordered_map<InstSeqNum, InstTimingInfo> timingMap;
};

} // namespace o3
} // namespace gem5

#endif // __CPU_O3_PROBE_INST_CSV_TRACE_HH__
