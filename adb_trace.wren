class EventHandler {
        static onBreakpoint() {
                var vctrCpltBank = Memory[0xE103DF]
                var vctrCplt = (Memory[0xE103DE] << 8) + Memory[0xE103DD]
                if (Register.K == 0xFC && Register.PC == 0xD9C5) System.print("*** AsyncADBReceive ***")
                if (Register.K == 0xE1 && Register.PC == 0x03DC) System.print("*** Completion vector ***")
                if (Register.K == vctrCpltBank && Register.PC == vctrCplt) System.print("*** VCTRCPLT ***")
                var mode = Register.emulationMode ? "E" : "N"
                System.write("@ K/PC=%(hexBank16(Register.K, Register.PC)) e:%(mode) - %(Register.emulationMode) ")
                System.write("SP=00/%(Hex.Str16(Register.SP)) DB=%(Hex.Str8(Register.DBR)) ")
                System.print("VCTRCPLT (E1/03DC) = %(hexBank16(vctrCpltBank, vctrCplt))")
        }

        static onFrameComplete() {

        }

        static hexBank16(bank, addr) {
                return "%(Hex.Str8(bank))/%(Hex.Str16(addr))"
        }
}

System.print("Break on AsyncADBReceive (FC/D9C5) and the completion vector (E1/03DC).")