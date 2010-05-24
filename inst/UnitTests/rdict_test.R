test_rdict_new <- function()
{
    d <- rdict_new()
    rm(d)
    gc()
}

test_rdict_put_get_basic <- function()
{
    set.seed(0x990)
    s <- seq_len(1000L)
    keys <- paste("k_", s, sep = "")
    vals <- sample(s)

    d <- rdict_new()
    for (i in seq_along(keys)) {
        rdict_put(d, keys[i], vals[i])
    }

    for (i in sample(seq_along(keys))) {
        v <- rdict_get(d, keys[i])
        checkEquals(vals[i], v)
    }
}

test_rdict_rm_basic <- function()
{
    set.seed(0x990)
    s <- seq_len(1000L)
    keys <- paste("k_", s, sep = "")
    vals <- sample(s)

    d <- rdict_new()
    for (i in seq_along(keys)) {
        rdict_put(d, keys[i], vals[i])
    }

    rm_list <- sample(keys, 300)
    for (k in rm_list) rdict_rm(d, k)
    for (k in rm_list) checkEquals(NULL, rdict_get(d, k))

    still_have <- seq_along(keys)[-(match(rm_list, keys))]
    for (i in still_have)
        checkEquals(vals[i], rdict_get(d, keys[i]))
}
