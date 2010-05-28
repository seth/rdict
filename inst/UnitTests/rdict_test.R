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

test_rdict_keys <- function()
{
    set.seed(0x990)
    s <- seq_len(1000L)
    keys <- paste("k_", s, sep = "")
    vals <- sample(s)

    d <- rdict_new()
    for (i in seq_along(keys)) {
        rdict_put(d, keys[i], vals[i])
    }

    got <- rdict_keys(d)
    checkEquals(sort(keys), sort(got))
}

test_rdict_mput <- function()
{
    set.seed(0x3440)
    ex1 <- seq_len(5000)
    names(ex1) <- paste("id_", ex1, sep="")
    ex1 <- as.list(ex1)
    d <- rdict_new()
    rdict_mput(d, ex1)
    checkTrue(setequal(names(ex1), rdict_keys(d)))
    for (k in sample(names(ex1))) {
        checkEquals(ex1[[k]], rdict_get(d, k))
    }
}
