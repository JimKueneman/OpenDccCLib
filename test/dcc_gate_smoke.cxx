/** @file dcc_gate_smoke.cxx
 *  @brief Trivial link driver for the compile-gate matrix.
 *
 *  This program does nothing. Its only job is to be linked against a per-config
 *  static library with whole-archive / -force_load, so EVERY object in that
 *  library is pulled into the link. If any translation unit references a symbol
 *  that is gated OFF in the active dcc_user_config.h, the link fails with an
 *  undefined reference — exactly the failure a real single-role firmware build
 *  would hit. Compile-time gate errors (a type/enum used when gated off) are
 *  caught earlier, when the library itself compiles.
 *
 *  See test/CMakeLists.txt (compile-gate matrix) for how it is wired.
 */

int main(void) {
    return 0;
}
