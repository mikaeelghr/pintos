# -*- perl -*-
use strict;
use warnings;
use tests::tests;
use tests::random;
check_expected (IGNORE_EXIT_CODES => 1, [<<'EOF']);
(join-cache) begin
(join-cache) create "garbage"
(join-cache) open "garbage"
(join-cache) disk count
(join-cache) disk count
(join-cache) write count ok
(join-cache) read count ok
(join-cache) end
EOF
pass;
