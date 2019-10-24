function bodysub() {
  sub(/\[\[[^]]*\]\] */, "")
  sub(/SpinSemaphore::/, "")
  gsub(/std::memory_order/, "memory_order")
  sub(/std::memory_order/, "memory_order")
  gsub(/nullptr/, "NULL")
  gsub(/bool/, "uint64_t")
  gsub(/true/, "1UL")
  gsub(/false/, "0UL")
  gsub(/AI_UNLIKELY/, "")
  gsub(/ noexcept$/, "")
  gsub(/DEBUG_ONLY\([^)]*\)/, "")
  gsub(/Futex<uint64_t>::/, "futex_")
  $0 = gensub(/([a-zA-Z0-9_]*)\.fetch_add\(([^)]*)\)/, "atomic_fetch_add_explicit(\\&\\1, \\2)", "g")
  $0 = gensub(/([a-zA-Z0-9_]*)\.load\(([^)]*)\)/, "atomic_load_explicit(\\&\\1, \\2)", "g")
  $0 = gensub(/([a-zA-Z0-9_]*)\.store\(([^)]*)\)/, "atomic_store_explicit(\\&\\1, \\2)", "g")
  $0 = gensub(/([a-zA-Z0-9_]*)\.compare_exchange_weak\(([^,]*), ([^,]*), ([^)]*)\)/, "atomic_compare_exchange_weak_explicit(\\&\\1, \\&\\2, \\3, \\4, \\4)", "g")

  # Uncomment to test with only seq_cst.
  #gsub(/memory_order_[a-z_]*/, "memory_order_seq_cst")
}
