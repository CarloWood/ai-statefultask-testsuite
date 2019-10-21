@include "genmc_prelude.awk"
@include "genmc_body.awk"

/int res;/ {
  next
}

/SpinSemaphore::slow_wait/,/^}$/ {
  bodysub()
  gsub(/.*futex_wait\(0\).*/, "      futex_wait(0)");
  gsub(/DelayLoop::delay_loop.*/, "delay_loop();");
  gsub(/\(\(ntokens == 0\)\)/, "(ntokens == 0)");
  print
}
