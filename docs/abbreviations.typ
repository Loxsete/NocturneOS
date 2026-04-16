#import "base.typ": *
#show: base-styles

#set document(title: "Abbreviations of words")

#align(center, title())


= Сокращения слов

#table(
  columns: 3,
  stroke: 0pt,
  gutter: 2pt,
  `dev`, [---], `device`,
  `nr`, [---], `number`,
  `f`, [---], `file`,
  `dir`, [---], `directory`,
  `blk`, [---], `block`,
  `phys`, [---], `physical`,
  `virt`, [---], `virtual`,
  `mem`, [---], `memory`,
  `k`, [---], `kernel`,
  `init`, [---], `initial`,
  `req`, [---], `request`,
  `ent`, [---], `entry`,
  `pg`, [---], `page`,
)


= Сокращения названий

#table(
  columns: 3,
  stroke: 0pt,
  gutter: 2pt,
  `mm`, [---], `memory manager`,
  `vmm`, [---], `virtual mm`,
  `pmm`, [---], `physical mm`,
  `memmap`, [---], `memory map`,
)