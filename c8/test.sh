./$1 $1.c test/$1_main1_ok.c && echo "$1.c" || echo "$1.c FAILED"

for fn in test/$1_*_ok.c; do
    ./$1 $fn arg_$fn && echo "$fn" || echo "$fn FAILED"
done

for fn in test/$1_*_fail.c; do
    ./$1 $fn arg_$fn && echo "$fn FAILED"
done
