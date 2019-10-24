@include "genmc_prelude.awk"
@include "genmc_body.awk"

/SpinSemaphore::slow_wait/,/^}$/ {
  bodysub()
  gsub(/.*futex_wait\(0\).*/, "      res = futex_wait(0)");
  gsub(/DelayLoop::delay_loop.*/, "atomic_load_explicit(\\&m_word, memory_order_relaxed);");
  gsub(/\(\(ntokens == 0\)\)/, "(ntokens == 0)");
  print
}
