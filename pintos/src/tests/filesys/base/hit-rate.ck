# -*- perl -*-
use strict;
use warnings;
use tests::tests;
use tests::random;
check_expected (IGNORE_EXIT_CODES => 1, [<<'EOF']);
(hit-rate) begin
(hit-rate) create "test"
(hit-rate) open "test"
(hit-rate) write 16384 bytes to "test"
(hit-rate) close "test"
(hit-rate) open "test"
(hit-rate) cacheinv
(hit-rate) cachestat
(hit-rate) read 16384 bytes from "test"
(hit-rate) cachestat
(hit-rate) close "test"
(hit-rate) open "test"
(hit-rate) read 16384 bytes from "test"
(hit-rate) cachestat
(hit-rate) old hit rate percent: 0, new hit rate percent: 94
(hit-rate) close "test"
(hit-rate) end
EOF
pass;
