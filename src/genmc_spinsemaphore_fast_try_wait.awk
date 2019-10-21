@include "genmc_prelude.awk"
@include "genmc_body.awk"

/uint64_t fast_try_wait/,/^  }$/ {
  bodysub()
  print
}
