// app: use-case orchestration and policy. No macOS headers in this layer.
//
// M2 belongs here: baseline handling and the "which files are new?" policy.
// Keep the watcher mechanism behind an interface so polling and FSEvents can
// be swapped without changing this code.
//
// See docs/ARCHITECTURE.md, milestones M2 and M3.
