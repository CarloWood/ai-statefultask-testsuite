@include "genmc_prelude.awk"
@include "genmc_body.awk"

/void post/,/^  }$/ {
  bodysub()
  gsub(/uint32_t n = 1/, "uint32_t n")
  print
}
