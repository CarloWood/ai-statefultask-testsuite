@include "genmc_prelude.awk"
@include "genmc_body.awk"

/void wait/,/^  }$/ {
  bodysub()
  print
}
