echo "\nTEST\n"

./$1 $1.c test/$1_main1_ok.c && echo "$1.c passed\n" || echo "$1.c FAILED\n"

for fn in test/$1_*_ok.c; do
    ./$1 $fn arg_$fn && echo "$fn passed\n" || echo "$fn FAILED\n"
done

for fn in test/$1_*_fail.c; do
    ./$1 $fn arg_$fn && echo "$fn FAILED\n" || echo "$fn passed\n"
done
