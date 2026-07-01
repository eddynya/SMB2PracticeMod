#include "menu_accel.h"
#include "../systems/pad.h"
#include "../systems/pref.h"
#include "../utils/patch.h"
#include "../utils/ppcutil.h"

namespace menu_accel {

void tick() {
    if (pref::get(pref::BoolPref::MenuAcceleration)) {
        // nop the instructions that check for R held down to accelerate the menu
        patch::write_nop(reinterpret_cast<void*>(0x80273478));  // for up inputs
        patch::write_nop(reinterpret_cast<void*>(0x802735C8));  // for down inputs
    } else {
        // Otherwise, recover the original instructions

        patch::write_word(reinterpret_cast<void*>(0x80273478),
                          PPC_INSTR_CMPWI(PPC_R3, 0));  // for up inputs

        // not sure why you need to use rlwinm instead of cmpwi for the down inputs
        patch::write_word(reinterpret_cast<void*>(0x802735C8),
                          PPC_INSTR_RLWINM(PPC_R3, PPC_R3, 0, 0x16, 0x16, 0));  // for down inputs
    }
}

}  // namespace menu_accel