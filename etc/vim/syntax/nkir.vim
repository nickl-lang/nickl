if exists("b:current_syntax")
  finish
endif

syn keyword nkirKeyword proc extern type data const pub local include
syn keyword nkirKeyword nop ret jmp jmpz jmpnz
syn keyword nkirKeyword call store load alloc mov ext trunc fp2i i2fp
syn keyword nkirKeyword add sub mul div mod and or xor lsh rsh
syn keyword nkirKeyword cmp eq ne lt le gt ge
syn keyword nkirKeyword comment
syn keyword nkirType i8 i16 i32 i64 u16 u32 u64 u8 f32 f64 void

syn match nkirComment    display '//.*$'
syn match nkirFunction   display '\<[a-zA-Z_]\+[(]'me=e-1
syn match nkirSymbol     display '#\w\+'
syn match nkirDollar     display '\$\w\+'
syn match nkirAt         display '@\w\+'
syn match nkirAt         display '@[-+][0-9]\+'
syn match nkirColon      display ':\w\+'
syn match nkirPercent    display '%\w\+'
syn match nkirNumber     display '\(0x[0-9a-fA-F]\+\|\.[0-9]\+\|\<[0-9]\+\.\=\d*\)\([eE][-+]\=\d\+\>\)\='

syn region nkirComment   start="/\*"    end="\*/"

" TODO Escape sequences

syn region nkirString start='"' end='"'

let b:current_syntax = "nkir"

hi def link nkirKeyword    Keyword
hi def link nkirNumber     Number
hi def link nkirString     String
hi def link nkirComment    Comment
hi def link nkirFunction   Function
hi def link nkirSymbol     PreProc
hi def link nkirDollar     Function
hi def link nkirAt         Special
hi def link nkirColon      Type
hi def link nkirPercent    PreProc
hi def link nkirType       Type
