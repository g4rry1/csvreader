
#include "Parse.h"
#include "Table.h"

int main(int argc, char *argv[]) {
  FILE *f = fopen(argv[1], "r");
  if (!f) {
    perror("fopen");
    return -1;
  }

  Table table;
  table_init(&table);
  if (!table.column_names) {
    fprintf(stderr, "Failed to initialize table\n");
    fclose(f);
    return -1;
  }
  if (!table.rows.buckets) {
    fprintf(stderr, "Failed to initialize table rows\n");
    table_destroy(&table);
    fclose(f);
    return -1;
  }
  parse_csv(f, &table);

  fclose(f);
  return 0;
}