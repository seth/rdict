rdict_new <- function()
{
    .Call(.rdict_new)
}

rdict_put <- function(rdict, key, value)
{
    invisible(.Call(.rdict_put, rdict, as.character(key), value))
}

rdict_mput <- function(rdict, aList)
{
    invisible(.Call(.rdict_mput, rdict, aList))
}

rdict_get <- function(rdict, key)
{
    .Call(.rdict_get, rdict, as.character(key))
}

rdict_rm <- function(rdict, key)
{
    .Call(.rdict_remove, rdict, as.character(key))
}

rdict_stats <- function(rdict)
{
    .Call(.rdict_stats, rdict)
}

rdict_keys <- function(rdict)
{
    .Call(.rdict_keys, rdict)
}

rdict_count <- function(rdict)
{
    .Call(.rdict_count, rdict)
}

