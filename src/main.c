
#include <stdio.h>

#include "Parse.h"
#include "Table.h"
#include "errors.h"

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <file.csv>\n", argv[0]);
        return ERR_PARSE;
    }
    FILE *f = fopen(argv[1], "r");
    if (!f) {
        perror(argv[1]);
        return ERR_PARSE;
    }

    Table table = {0};
    int err = parse_csv(f, &table);
    if (err != ERR_OK) {
        table_destroy(&table);
        fclose(f);
        return err;
    }

    err = evaluate_all(&table);
    if (err != ERR_OK) {
        table_destroy(&table);
        fclose(f);
        return err;
    }

    table_print(&table, stdout);
    table_destroy(&table);
    fclose(f);
    return ERR_OK;
}
