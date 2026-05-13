
#include "Parse.h"
#include "Table.h"

int main(int argc, char *argv[]) {
  (void)argc;
  FILE *f = fopen(argv[1], "r");
  if (!f) {
    perror("fopen");
    return -1;
  }

  Table table = {0};
  if (parse_csv(f, &table)) {
    fprintf(stderr, "Failed to parse CSV\n");
    table_destroy(&table);
    fclose(f);
    return -1;
  }

  if (evaluate_all(&table)) {
    fprintf(stderr, "Failed to evaluate all cells\n");
    table_destroy(&table);
    fclose(f);
    return -1;
  }

  table_print(&table, stdout);
  table_destroy(&table);
  fclose(f);
  return 0;
}