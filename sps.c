/******************************************************

* PROJEKT 2 - PRACE S DATOVYMI STRUKTURAMI
* PREDMET IZP 2020/21
* AUTOR: Lucie Svobodova, VUT login: xsvobo1x

******************************************************/

/**
  * Syntax spusteni programu:
  * ./sps [-d DELIM] CMD_SEQUENCE FILE
**/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <ctype.h>

enum selection {R1, C1, R2, C2};
enum return_codes {SUCCESS, FAILURE, NOT_USED = -1};

typedef struct {
    unsigned len;
    char *value;
} Cell;

typedef struct {
    unsigned num_of_cells;
    Cell *cell;
} Row;

typedef struct {
    unsigned num_of_rows;
    unsigned num_of_cols;
    Row *row;
} Table;

Table table_ctor()
{
    Table table = {
        .num_of_rows = 0,
        .num_of_cols = 0,
        .row = NULL
    };
    return table;
}

Row row_ctor()
{
    Row row = {
        .num_of_cells = 0,
        .cell = NULL
    };
    return row;
}

// Funkce uvolni jednu bunku
void cell_dtor(Cell *cell)
{
    cell->len = 0;
    free(cell->value);
    cell->value = NULL;
}

// Funkce uvolni jeden radek
void row_dtor(Row *row)
{
    for(unsigned c = 0; c < row->num_of_cells; c++) {
        cell_dtor(&row->cell[c]);
    }
    free(row->cell);
    row->num_of_cells = 0;
    row->cell = NULL;
}

// Funkce uvolni celou tabulku
void table_dtor(Table *table)
{
    for(unsigned r = 0; r < table->num_of_rows; r++) {
        row_dtor(&table->row[r]);
    }
    free(table->row);
}

// Funkce inicializuje bunku
// Pokud se nepodari alokovat pamet, vypise hlaseni a vrati FAILURE
int cell_init(Cell *cell)
{
    cell->value = malloc(sizeof(char));
    if (cell->value == NULL) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        return FAILURE;
    }
    cell->len = 0;
    return SUCCESS;
}

// Funkce inicializuje radek row a bunku c na nem
// Pokud se nepodari alokovat pamet, vypise hlaseni a vrati FAILURE
int row_init(Row *row, unsigned c)
{
    row->cell = malloc(sizeof(Cell));
    if (row->cell == NULL) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        return FAILURE;
    }
    if (cell_init(&row->cell[c])) {
        if(c)
            row->cell = realloc(row->cell, --row->num_of_cells);
        else
            free(row->cell);
        return FAILURE;
    }
    row->num_of_cells = c + 1;
    return SUCCESS;
}

// Funkce inicializuje tabulku - prvni radek s prvni bunkou na nem
// Pokud se nepodari alokovat pamet, vraci FAILURE
int table_init(Table *table)
{
    table->row = malloc(sizeof(Row));
    if (table->row == NULL) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        return FAILURE;
    }
    table->num_of_rows = 0;

    if (row_init(&table->row[0], 0)) {
        free(table->row);
        return FAILURE;
    }
    table->row[0].num_of_cells = 1;
    table->row[0].cell[0].len = 0;
    return SUCCESS;
}

// Funkce realokuje pamet pro bunku c na radku row na velikost row->cell[c].len
// Pokud se nepodari, uvolni bunku, vypise hlaseni a vrati FAILURE
int cell_resize(Row *row, unsigned c)
{
    char *ptr = realloc(row->cell[c].value, row->cell[c].len * sizeof(char));
    if (ptr == NULL) {
        free(row->cell[c].value);
        row->cell = realloc(row->cell, --row->num_of_cells);
        fprintf(stderr, "Error: %s\n", strerror(errno));
        return FAILURE;
    }
    row->cell[c].value = ptr;
    return SUCCESS;
}

// Funkce realokuje pamet pro radek row na velikost n
// Pri neuspechu vraci FAILURE
int row_resize(Row *row, unsigned n)
{
    Cell *ptr = realloc(row->cell, n * sizeof(Cell));
    if (ptr == NULL) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        return FAILURE;
    }
    row->cell = ptr;
    row->num_of_cells = n;
    return SUCCESS;
}

// Funkce alokuje pamet pro n novych radku v tabulce
// Pri neuspechu vraci FAILURE
int table_resize(Table *table, unsigned n)
{
    Row *ptr  = realloc(table->row, (table->num_of_rows + n) * sizeof(Row));
    if (ptr == NULL) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        return FAILURE;
    }
    table->row = ptr;
    table->num_of_rows += n;
    return SUCCESS;
}

// Funkce zkontroluje zadane command line argumenty
// V pripade chyby vypise hlaseni a vrati FAILURE
int process_arguments(int argc, char **argv)
{
    if ((argc != 3 && argc != 5) || (argc == 5 && strcmp(argv[1], "-d"))) {
        fprintf(stderr, "Error: %s\n", strerror(EINVAL));
        fprintf(stderr, "Usage: ./sps [-d DELIM] CMD_SEQUENCE FILE\n");
        return FAILURE;
    }
    return SUCCESS;
}

// Funkce zkontroluje, zda je retezec oddelovacu ve spravnem formatu
// Pokud obsahuje \ nebo ", vrati false
bool check_delim(char *delim)
{
    for (unsigned i = 0; delim[i] != '\0'; i++) {
        if (delim[i] == '\\')
            return false;
        if (delim[i] == '\"')
            return false;
    }
    return true;
}

// Funkce ulozi retezec oddelovacu do delim
void get_delim(int argc, char **argv, char **delim)
{
    if (argc == 5) {
        *delim = argv[2];
        return;
    }
    *delim = " ";
}

// Funkce zkontroluje, zda je znak c oddelovacem
bool is_delim(char c, char *delim)
{
    for(unsigned i = 0; delim[i] != '\0'; i++) {
        if (c == delim[i])
            return true;
    }
    return false;
}

// Funkce vytiskne bunku [r,c] do souboru file ve vystupnim formatu
void cell_print(FILE *file, Table table, unsigned r, unsigned c, char *delim)
{
    char buffer[1000] = "";
    bool quotes = false;

    // pokud bunka neni inicializovana, nevypise se
    if (c >= table.row[r].num_of_cells || table.row[r].cell[c].value == NULL)
        return; 
    for (unsigned i = 0; i < table.row[r].cell[c].len; i++) {
        if (is_delim(table.row[r].cell[c].value[i], delim))
            quotes = true;
        else if (table.row[r].cell[c].value[i] == '\"')
            strcat(buffer, "\\");
        else if (table.row[r].cell[c].value[i] == '\\')
            strcat(buffer, "\\");
        strncat(buffer, &table.row[r].cell[c].value[i], 1);
    }
    if (quotes) {
        fputc('\"', file);
        fputs(buffer, file);
        fputc('\"', file);
    } else
        fputs(buffer, file);
}

// Funkce odstrani posledni sloupec, pokud je cely prazdny
void delete_last_col(Table *table)
{
    for (unsigned r = 0; r < table->num_of_rows; r++) {
        if (table->row[r].num_of_cells == table->num_of_cols) {
            cell_dtor(&table->row[r].cell[table->num_of_cols - 1]);
            table->row[r].num_of_cells--;
        }
    }
    table->num_of_cols--;
}

void check_last_cols(Table *table)
{
    while (1) {
        for (unsigned r = 0; r < table->num_of_rows; r++) {
            //pokud neni posledni bunka pradna, empty se nastavi na false
            if (table->row[r].num_of_cells == table->num_of_cols) {
                if (table->row[r].cell[table->num_of_cols - 1].len)
                    // pokud bunka neni prazdna, funkce je ukoncena
                    return;
            }
        }
        delete_last_col(table);
    }
}

// Funkce vytiskne tabulku do souboru file ve vystupnim formatu
void table_print(FILE *file, Table table, char *delim)
{
    check_last_cols(&table);
    for (unsigned r = 0; r < table.num_of_rows; r++) {
        for (unsigned c = 0; c < table.num_of_cols; c++) {
            if (c)
                fputc(delim[0], file);
            cell_print(file, table, r, c, delim);
        }
        if (r != table.num_of_rows - 1)
            fputc('\n', file);
    }
}

// Funkce nacte jeden radek tabulky do pameti
// Pri neuspesne alokaci pameti vraci FAILURE
int row_load(Row *row, char *line, char *delim)
{
    unsigned c = 0;
    bool bckslash = false;
    bool quotes = false;
    char buf[1000];
    memset(buf, 0, 1000);

    for(unsigned i = 0; line[i] != 0; i++) {
        if (bckslash)
            bckslash = false;
        else if (line[i] == '"') {
            quotes = !quotes;
            continue;          
        } else if (line[i] == '\\') {
            bckslash = true;
            continue;
        } else if (quotes)
            ; 
        else if (is_delim(line[i], delim)) {
            row->cell[c].len++;
            // alokace pameti pro danou delku nacitane bunky
            if (cell_resize(row, c))
                return FAILURE;
            // ulozeni hodnoty do bunky a vymazani bufferu
            memcpy(row->cell[c].value, buf, row->cell[c].len);
            memset(buf, 0, row->cell[c].len);
            // zvetseni pameti pro 1 novou bunku
            if (row_resize(row, row->num_of_cells + 1))
                return FAILURE;
            if (cell_init(&row->cell[++c]))
                return FAILURE;
            continue;
        } else if (line[i] == '\n')
            break;
        strncat(buf, &line[i], 1);    
        row->cell[c].len++;
    }
    row->cell[c].len++;
    if (cell_resize(row, c))
        return FAILURE;
    memcpy(row->cell[c].value, buf, row->cell[c].len);
    return SUCCESS;
}

// Function nacte tabulku ze souboru file do pameti
int table_load(FILE *file, Table *table, char *delim)
{
    char line[100002];
    if (fgets(line, 100000, file)) {
        table->num_of_cols = 1;
        table->num_of_rows = 1;
    }
    // kontrola prazdneho souboru
    char x;
    if ((x = fgetc(file)) == EOF) {
        return  FAILURE;
    }
    ungetc(x, file);
   
    unsigned r = 0;
    do {
        if (row_load(&table->row[r], line, delim))
            return FAILURE;
        if (table->row[r].num_of_cells > table->num_of_cols)
            table->num_of_cols = table->row[r].num_of_cells;
        // kontrola, zda nacitany radek neni posledni
        char x;
        if ((x = fgetc(file)) == EOF)
            break;
        ungetc(x, file);
        // zvetseni mista pro novy radek v tabulce
        if (table_resize(table, 1))
            return FAILURE;
        if (row_init(&table->row[++r], 0))
            return FAILURE;
    } while(fgets(line, 10000, file));
    return SUCCESS;
}

// Funkce nacte command line argumenty do pameti
int commands_load(Row *arg, char *str)
{
    if (row_load(arg, str, ";"))
        return FAILURE;
    if (arg->num_of_cells >= 1000) {
        fprintf(stderr, "Too many commands!\n");
        return FAILURE;
    }
    for (unsigned i = 0; i < arg->num_of_cells; i++) {
	if (arg->cell[i].len >= 1000) {
	    fprintf(stderr, "Max length of command is 1000\n");
	    return FAILURE;
	}
    }
    return SUCCESS;
}


// Funkce najde bunku s minimalni hodnotou a vyber nastavi na ni
int min(Table table, unsigned *sel)
{
    double min;
    bool first = true;
    for (unsigned r = sel[R1]; r <= sel[R2]; r++) {
        for (unsigned c = sel[C1]; c <= sel[C2]; c++) {
            if (c >= table.row[r].num_of_cells || table.row[r].cell[c].len == 0)
                continue;
            char *ptr;
            errno = 0;
            double num = strtod(table.row[r].cell[c].value, &ptr);
            if (ptr == table.row[r].cell[c].value || errno)
                continue;
            if (first) {
                first = false;
                min = num;
                sel[R1] = r;
                sel[C1] = c;
            } else if (num < min) {
                min = num;
                sel[R1] = r;
                sel[C1] = c;
            }
        }
    }
    sel[R2] = sel[R1];
    sel[C2] = sel[C1];
    return SUCCESS;
}

// Funkce najde bunku s maximalni hodnotou a vyber nastavi na ni
int max(Table table, unsigned *sel)
{
    double max;
    bool first = true;
    for (unsigned r = sel[R1]; r <= sel[R2]; r++) {
        for (unsigned c = sel[C1]; c <= sel[C2]; c++) {
            if (c >= table.row[r].num_of_cells || table.row[r].cell[c].len == 0)
                continue;
            char *ptr;
            errno = 0;
            double num = strtod(table.row[r].cell[c].value, &ptr);
            if (ptr == table.row[r].cell[c].value || errno)
                continue;
            //if (r == sel[R1] && c == sel[C1]) {
            if (first) {
                first = false;
                max = num;
                sel[R1] = r;
                sel[C1] = c;
            } else if (num > max) {
                max = num;
                sel[R1] = r;
                sel[C1] = c;
            }
        }
    }
    sel[R2] = sel[R1];
    sel[C2] = sel[C1];
    return SUCCESS;
}

// Funkce prevede retezec str podle vstupniho formatu
void check_str_format(char *str)
{
    unsigned j = 0;
    bool bckslash = false;
    bool quotes = false;
    for(unsigned i = 0; str[i] != 0; i++) {
        if (bckslash)
            bckslash = false;
        else if (str[i] == '"') {
            quotes = !quotes;
            continue;          
        } else if (str[i] == '\\') {
            bckslash = true;
            continue;
        } else if (quotes) {
            ; 
        }
        str[j] = str[i];
        j++;
    }
    str[j] = '\0';

}

// Funkce zkontroluje, zda retezec str zacina retezcem cmd
bool check_str(char *cmd, char *str)
{
    if (str[0] == '\0')
	return false;
    for (unsigned i = 0; str[i] != '\0'; i++) {
        if (cmd[i] == '\0')
            return true;
        else if (cmd[i] != str[i])
            return false;
    }
    return true;
}

// Funkce zkontroluje, zda retezec str obsahuje podretezec str_to_find
bool contains_str(char *str, char *str_to_find)
{
    if (str[0] == '\0' && str_to_find[0] == '\0')
        return true;
    for (unsigned i = 0; str[i] != '\0'; i++) {
        if (str[i] == str_to_find[0]) {
            unsigned j = i;
            for (unsigned k = 0; str_to_find[k] != '\0'; k++) {
                if (str[j] != str_to_find[k])
                    break;
                if (str[j + 1] == '\0' && str_to_find[k + 1] != '\0')
                    return false;
                j++;
                if (str_to_find[k + 1] == '\0')
                    return true;
            }
        }
    }
    return false;
}

// Funkce v selekci najde bunku obsahujici podretezec str a vybec nastavi na ni
// Pokud neni takova bunka nalezena, je vyber ponechan na puvodni selekci
int find_str(Table table, unsigned *sel, char *cmd)
{
    char *str = strstr(cmd, " ");
    str++;
    str[strlen(str) - 1] = '\0';
    check_str_format(str);
    for (unsigned r = sel[R1]; r <= sel[R2]; r++) {
        for (unsigned c = sel[C1]; c <= sel[C2]; c++) {
            if (c >= table.row[r].num_of_cells || table.row[r].cell[c].len == 0)
                continue;
            if (contains_str(table.row[r].cell[c].value, str)) {
                sel[R1] = r;
                sel[C1] = c;
                sel[R2] = r;
                sel[C2] = c;
                return 0;
            }
        }
    }
    return 0;
}

// Pokud je jako selekce zadana nektera z funkci, je provedena
// Pokud zadna z funkci zadana neni, funkce vraci NOT_USED
int sel_func(Table table, char *cmd, unsigned *sel)
{
    // [set] - kopie soucasne selekce do promenne _ (sel[4] az sel[7])
    if (strcmp(cmd, "[set]") == 0) {
        unsigned j = 0;
        for (unsigned i = 4; i < 8; i++) {
            sel[i] = sel[j++];
        }
    // [_] - nastaveni selekce ze promenne _ (sel[4] az sel[7])
    } else if (strcmp(cmd, "[_]") == 0) {
        unsigned j = 0;
        for (unsigned i = 4; i < 8; i++) {
            sel[j++] = sel[i];
        }
    // [min] - najde bunku s minimalni hodnotou a vyber nastavi na ni
    } else if (strcmp(cmd, "[min]") == 0) {
        if (min(table, sel))
            return FAILURE;
    // [max] - najde bunku s maximalni hodnotou a vyber nastavi na ni
    } else if (strcmp(cmd, "[max]") == 0) {
        if (max(table, sel))
            return FAILURE;
    // [find STR] - vyber nastavi na prvni bunku obsahujici podretezec STR
    // pokud zadna takova bunka ve vyberu neni, puvodni selekce je ponechana
    } else if (check_str(cmd, "[find ")) {
        find_str(table, sel, cmd);
    } else
        // zadna funkce nebyla vykonana
        return NOT_USED;
    return SUCCESS;
}

// Funkce vraci 0 a nastavi selekci, pokud je selekce zadana v prikazu spravne
// Selekce znovu
int set_selection(Table table, Cell cmd, unsigned *sel)
{
    bool col = false;
    bool row = false;
    int n = 0;
    long int num = 0;
    char *ptr = cmd.value;
    // vykonani funkci selekce - [set], [_], [min], [max], [find STR]
    int fce;
    if ((fce = sel_func(table, cmd.value, sel)) != -1) {
        if (fce)
            return FAILURE;
        return SUCCESS;
    }
    while (*ptr) {
        if (isdigit(*ptr)) {
            num = strtoul(ptr, &ptr, 10) - 1;
	    if (num < 0)
		return FAILURE;
	    sel[n++] = num;
        } else if (*ptr == '_') {
            // [_,C] - cely sloupec C
            if (n == R1) {
                row = true;
                sel[R1] = 0;
                sel[R2] = table.num_of_rows - 1;
                ptr++;
                n++;
            }
            // [R,_] - cely radek R
            else if (n == C1) {
                col = true;
                sel[C1] = 0;
                sel[C2] = table.num_of_cols - 1;
                ptr++;
                n++;
            }
        } else if (*ptr == '-') {
            // okno - R2 nebo C2
            if (n == R2 && !row && !col) {
                sel[R2] = table.num_of_rows - 1;
                n++;
                ptr++;
            } else if (n == C2 && !row && !col) {
                sel[C2] = table.num_of_cols - 1;
                n++;
                ptr++;
            } else
                return 1;
        } else {
            ptr++;
        }
    }
    if (n == 4 && sel[R1] <= sel[R2] && sel[C1] <= sel[C2])
        return SUCCESS;
    // [R,C]
    else if (n == 2) {
        if (row && col)
            return SUCCESS;
        else {
            if (!col)
                sel[C2] = sel[C1];
            if (!row)
            sel[R2] = sel[R1];
        }
        return SUCCESS;
    } 
    return FAILURE;
}

// Funkce zjisti, zda je zadana bunka selekci (zacina [ a konci ])
bool is_selection(Cell cmd)
{
    if ((cmd.value[0] == '[') && (cmd.value[cmd.len - 2] == ']'))
        return true;
    return false;
}

// Funkce prida radek pred radek sel[R1]
int irow(Table *table, unsigned *sel)
{
    if (table_resize(table, 1))
        return FAILURE;
    for(unsigned r = table->num_of_rows - 1; r > sel[R1]; r--) {
        table->row[r] = table->row[r - 1];
    }
    if (row_init(&table->row[sel[R1]], 0))
        return FAILURE;
    return SUCCESS;
}

// Funkce prida radek za radek sel[R2]
int arow(Table *table, unsigned *sel)
{
    if (table_resize(table, 1))
        return FAILURE;
    for (unsigned r = table->num_of_rows - 2; r > sel[R2]; r--) {
        table->row[r + 1] = table->row[r];
    }
    if (row_init(&table->row[sel[R2] + 1], 0))
        return FAILURE;
    return SUCCESS;
}

// Funkce odstrani radky v selekci (sel[R1] az sel[R2])
int drow(Table *table, unsigned *sel)
{
    for(unsigned r = sel[R1]; r < sel[R2] + 1; r++) {
        row_dtor(&table->row[r]);
    }
    for(unsigned r = sel[R1]; r < table->num_of_rows - (sel[R2] - sel[R1] + 1); r++) {
        table->row[r] = table->row[r + (sel[R2] - sel[R1] + 1)];
    }
    if (table_resize(table, sel[R2] - sel[R1] - 1))
        return FAILURE;
    return SUCCESS;
}

// Funkce prida sloupec pred sloupec sel[C1]
int icol(Table *table, unsigned *sel)
{
    for (unsigned r = 0; r < table->num_of_rows; r++) {
        if (row_resize(&table->row[r], table->row[r].num_of_cells + 1))
            return FAILURE;
        for(unsigned c = table->row[r].num_of_cells - 1; c > sel[C1];  c--) {
            table->row[r].cell[c] = table->row[r].cell[c - 1];
        }
            if (cell_init(&table->row[r].cell[sel[C1]]))
                return FAILURE;
        if (table->row[r].num_of_cells > table->num_of_cols)
            table->num_of_cols = table->row[r].num_of_cells;
    }
    return SUCCESS;
}

// Funkce prida sloupec za sloupec sel[C2]
int acol(Table *table, unsigned *sel)
{
    for (unsigned r = 0; r < table->num_of_rows; r++) {
        if (row_resize(&table->row[r], table->row[r].num_of_cells + 1))
            return FAILURE;
        for (unsigned c = table->row[r].num_of_cells - 2; c > sel[C2]; c--) {
            table->row[r].cell[c + 1] = table->row[r].cell[c];
        }
        if (cell_init(&table->row[r].cell[sel[C2] + 1]))
            return FAILURE;
        if (table->row[r].num_of_cells > table->num_of_cols)
            table->num_of_cols = table->row[r].num_of_cells;
    }
    return SUCCESS;
}

// Funkce odstrani sloupce v selekci (sel[C1] az sel[C2])
int dcol(Table *table, unsigned *sel)
{
    unsigned r, c;
    for (r = 0; r < table->num_of_rows; r++) {
        for (c = sel[C1]; c < sel[C2] + 1; c++) {
            cell_dtor(&table->row[r].cell[c]);
            table->row[r].cell[c].value = NULL;
        }
    }
    for (r = 0; r < table->num_of_rows; r++) {
        for (c = sel[C1]; c < table->row[r].num_of_cells - (sel[C2] - sel[C1] + 1); c++) {
            table->row[r].cell[c] = table->row[r].cell[c + (sel[C2] - sel[C1] + 1)];
        }
        if (row_resize(&table->row[r], table->row[r].num_of_cells + (sel[C2] - sel[C1] - 1)))
            return FAILURE;
    }
    table->num_of_cols -= (sel[C2] - sel[C1]);
    return SUCCESS;
}

// Funkce vymaze obsah bunek v selekci
int clear(Table *table, unsigned *sel)
{
    for (unsigned r = sel[R1]; r < sel[R2] + 1; r++) {
        for (unsigned c = sel[C1]; c < sel[C2] + 1; c++) {
            table->row[r].cell[c].len = 1;
            if (cell_resize(&table->row[r], c))
                return FAILURE;
            table->row[r].cell[c].value[0] = '\0';
        }
    }
    return SUCCESS;
}

// Funkce provede prislusnou funkci, pokud je zadana jako prikaz
// Pokud zadna z funkci neni prikazem, vraci NOT_USED, pri nedostatku pameti FAILURE
int choose_function0(Table *table, char *cmd, unsigned *sel)
{
    char *def_func[7] = {"irow", "arow", "drow", "icol", "acol", "dcol", "clear"};
    int (*fun_ptrs[])(Table*, unsigned*) = {irow, arow, drow, icol, acol, dcol, clear};

    for(unsigned i = 0; i < 7; i++) {
        if (strcmp(cmd, def_func[i]) == 0) {
            if ((*fun_ptrs[i])(table, sel))
                return FAILURE;
            return SUCCESS;
        }
    }
    return NOT_USED;
}

// Funkce z prikazu cmd ulozi do pole point polohu bunky
int extract_point(char *cmd, char *str, unsigned *point)
{
    char *ptr;
    if ((ptr = strstr(cmd, str)) == NULL)
        return FAILURE;
    int n = 0;
    long int num = 0;
    while (*ptr) {
        if (isdigit(*ptr)) {
            num = strtoul(ptr, &ptr, 10) - 1;
	    if (num < 0)
		return FAILURE;
	    point[n++] = num;
        } else
            ptr++;
    }
    if (n != 2) {
        printf("Command %s is not valid.\n", cmd);
        return FAILURE;
    }
    return SUCCESS;
}

// Funkce secte hodnotu bunek v selekci a vysledek ulozi do bunky [point[0],point[1]]
int sum(Table *table, char *cmd, unsigned *sel)
{
    unsigned point[2];
    // ulozi souradnice bunky z prikazu do point
    if (extract_point(cmd, "sum ", point))
        return FAILURE;
    if (point[0] >= table->num_of_rows || point[1] >= table->row[point[0]].num_of_cells)
        return FAILURE;
    double sum = 0.0;
    for (unsigned r = sel[R1]; r <= sel[R2]; r++) {
        for (unsigned c = sel[C1]; c <= sel[C2]; c++) {
            char *ptr;
            unsigned num = strtod(table->row[r].cell[c].value, &ptr);
            if (ptr == NULL)
                continue;
            sum += num;
        }
    }
    char result[1000] = "";
    sprintf(result, "%g", sum);
    table->row[point[0]].cell[point[1]].len = strlen(result) + 1;
    if (cell_resize(&table->row[point[0]], point[1]))
        return FAILURE;
    memcpy(table->row[point[0]].cell[point[1]].value, result, table->row[point[0]].cell[point[1]].len);
    return SUCCESS;
}

// Funkce vypocte prumer bunek v selekci a vysledek ulozi do bunky [point[0],point[1]]
int avg(Table *table, char *cmd, unsigned *sel)
{
    unsigned point[2];
    // ulozi souradnice bunky z prikazu do point
    if (extract_point(cmd, "avg ", point))
        return FAILURE;
    if (point[0] >= table->num_of_rows || point[1] >= table->row[point[0]].num_of_cells)
        return FAILURE;
    double sum = 0.0;
    double count = 0.0;
    for (unsigned r = sel[R1]; r <= sel[R2]; r++) {
        for (unsigned c = sel[C1]; c <= sel[C2]; c++) {
            char *ptr;
            unsigned num = strtod(table->row[r].cell[c].value, &ptr);
            if (ptr == NULL)
                continue;
            sum += num;
            count++;
        }
    }
    char result[1000] = "";
    sprintf(result, "%g", sum / count);
    table->row[point[0]].cell[point[1]].len = strlen(result) + 1;
    if (cell_resize(&table->row[point[0]], point[1]))
        return FAILURE;
    memcpy(table->row[point[0]].cell[point[1]].value, result, table->row[point[0]].cell[point[1]].len);
    return SUCCESS;
}

// Funkce spocte neprazdne bunky v selekci a vysledek ulozi do bunky [point[0],point[1]]
int count(Table *table, char *cmd, unsigned *sel)
{
    unsigned point[2];
    // ulozi souradnice bunky z prikazu do point
    if (extract_point(cmd, "count ", point))
        return FAILURE;
    if (point[0] >= table->num_of_rows || point[1] >= table->row[point[0]].num_of_cells)
        return FAILURE;
    double count_cell = 0.0;
    for (unsigned r = sel[R1]; r <= sel[R2]; r++) {
        for (unsigned c = sel[C1]; c <= sel[C2]; c++) {
            if (table->row[r].cell[c].value[0] != '\0')
                count_cell++;
        }
    }
    char result[1000] = "";
    sprintf(result, "%g", count_cell);
    table->row[point[0]].cell[point[1]].len = strlen(result) + 1;
    if (cell_resize(&table->row[point[0]], point[1]))
        return FAILURE;
    memcpy(table->row[point[0]].cell[point[1]].value, result, table->row[point[0]].cell[point[1]].len);
    return SUCCESS;
}

// Funkce do bunky [point[0],point[1]] ulozi delku prvni bunky v selekci
int len(Table *table, char *cmd, unsigned *sel)
{
    unsigned point[2];
    // ulozi souradnice bunky z prikazu do point
    if (extract_point(cmd, "len", point))
        return FAILURE;
    if (point[0] >= table->num_of_rows || point[1] >= table->row[point[0]].num_of_cells)
        return FAILURE;
    
    char result[1000] = "";
    sprintf(result, "%lu", strlen(table->row[sel[R1]].cell[sel[C1]].value));
    table->row[point[0]].cell[point[1]].len = strlen(result) + 1;
    if (cell_resize(&table->row[point[0]], point[1]))
        return FAILURE;
    memcpy(table->row[point[0]].cell[point[1]].value, result, table->row[point[0]].cell[point[1]].len);
    return SUCCESS;
}

// Funkce zameni dva ukazatele na bunky
void swap_cells(Cell *a, Cell *b)
{
    Cell temp = *a;
    *a = *b;
    *b = temp;
}

// Funkce zameni obsah bunky v selekci s bunkou [point[0],point[1]]
int swap(Table *table, Cell *cell, char *cmd)
{
    unsigned point[2];
    // ulozi souradnice bunky z prikazu do point
    if (extract_point(cmd, "swap", point))
        return FAILURE;
    if (point[0] >= table->num_of_rows || point[1] >= table->row[point[0]].num_of_cells)
        return FAILURE;
    
    swap_cells(&table->row[point[0]].cell[point[1]], cell);
    return SUCCESS;
}

// Funkce provede prislusnou funkci, pokud je zadana jako prikaz
// Pokud zadna z funkci neni prikazem, vraci NOT_USED, pri nedostatku pameti FAILURE
int choose_function1(Table *table, char *cmd, unsigned *sel)
{
    char *def_func[4] = {"sum", "avg", "count", "len"};
    int (*fun_ptrs[])(Table*, char*, unsigned*) = {sum, avg, count, len};
    bool exe = false;
    for (unsigned i = 0; i < 4; i++) {
        if (check_str(cmd, def_func[i])) {
            if ((*fun_ptrs[i])(table, cmd, sel))
                return FAILURE;
            exe = true;
        }
    }
    if (exe)
        return SUCCESS;
    return NOT_USED;
}

// Hodnotu bunky cell nastavi na str, pokud se nepodari alokovat pamet, vraci FAILURE
int set_cell_str(Row *row, unsigned c, char *str)
{
    row->cell[c].len = strlen(str) + 1;
    if (cell_resize(row, c))
        return FAILURE;
    memcpy(row->cell[c].value, str, row->cell[c].len);
    return SUCCESS;
}

// Funkce nastavi do vsech vybranych bunek retezec str
// Pokud neni provedena, vraci NOT_USED, pri nedostatku pameti vraci FAILURE
int set_str(Table *table, char *cmd, unsigned *sel)
{
    if (!check_str(cmd, "set "))
        return NOT_USED;
    char *str = strstr(cmd, " ");
    str++;
    check_str_format(str);
    for (unsigned r = sel[R1]; r <= sel[R2]; r++) {
        for (unsigned c = sel[C1]; c <= sel[C2]; c++) {
            if (set_cell_str(&table->row[r], c, str))
               return FAILURE;
        }
    }
    return SUCCESS;
}

// Funkce do num ulozi cislo docasne promenne z prikazu
int extract_tmp_var(char *cmd, unsigned *num)
{
    char *ptr;
    if ((ptr = strstr(cmd, "_")) == NULL)
        return FAILURE;
    ptr++;
    unsigned n = strtoul(ptr, &ptr, 10);
    if (ptr[0] == '\0' && n < 10) {
        *num = n;
        return SUCCESS;
    }
    return FAILURE;
}
// Funkce nastavi hodnotu bunky v selekci do docasne promenne
int def(Table *table, unsigned r, unsigned c, Row *tmp_var, char *cmd)
{
    unsigned num;
    if (extract_tmp_var(cmd, &num))
        return FAILURE;
    if (num >= 10)
        return FAILURE;
    // inicializace promenne, pokud jeste neni inicializovana
    if (num >= tmp_var->num_of_cells) {
        unsigned num_of_cells_bef = tmp_var->num_of_cells;
        if (row_resize(tmp_var, num + 1))
            return FAILURE;
        for (unsigned i = num_of_cells_bef; i <= num; i++) {
            if (cell_init(&tmp_var->cell[i]))
                return FAILURE;
	}
    }
    tmp_var->cell[num].len = table->row[r].cell[c].len;
    if (cell_resize(tmp_var, num))
        return FAILURE;
    memcpy(tmp_var->cell[num].value, table->row[r].cell[c].value, tmp_var->cell[num].len);
    return SUCCESS;
}

// Funkce nastavi bunku v selekci na hodnotu prislusne docasne promenne
int use(Table *table, unsigned r, unsigned c, Row *tmp_var, char *cmd)
{
    unsigned num;
    if (extract_tmp_var(cmd, &num))
        return FAILURE;
    if (num >= tmp_var->num_of_cells || tmp_var->cell[num].len == 0) {
        return FAILURE;
    }
    table->row[r].cell[c].len = tmp_var->cell[num].len;
    if (cell_resize(&table->row[r], c))
        return FAILURE;
    memcpy(table->row[r].cell[c].value, tmp_var->cell[num].value, table->row[r].cell[c].len);
    return SUCCESS;
}

// Funkce inkrementuje numerickou hodnotu v docasne promenne
// Pokud docasna promenna neobsahuje numerickou hodnotu, je nastavena na 1
int inc(Row *tmp_var, char *cmd)
{
    unsigned num;
    if (extract_tmp_var(cmd, &num))
        return FAILURE;
    if (num >= tmp_var->num_of_cells || tmp_var->cell[num].len == 0) {
        return FAILURE;
    }
    char *ptr;
    double res = strtod(tmp_var->cell[num].value, &ptr);
    if (ptr[0] == '\0')
       res++;
    else
        res = 1.0;
    
    char result[1000] = "";
    sprintf(result, "%g", res);
    tmp_var->cell[num].len = strlen(result) + 1;
    if (cell_resize(tmp_var, num))
        return FAILURE;
    memcpy(tmp_var->cell[num].value, result, tmp_var->cell[num].len);
    return SUCCESS;
}

// Funkce provede prislusnou funkci, pokud je zadana jako prikaz
// Pokud zadna z funkci neni prikazem, vraci NOT_USED, pri nedostatku pameti FAILURE
int tmp_var_function(Table *table, char *cmd, Row *tmp_var, unsigned *sel)
{
    char *def_func[] = {"def ", "use "};
    int (*func_ptrs[])(Table*, unsigned, unsigned, Row*, char*) = {def, use};

    for (unsigned i = 0; i < 3; i++) {
        if (check_str(cmd, def_func[i])) {
            if ((*func_ptrs[i])(table, sel[R1], sel[C1], tmp_var, cmd))
                return FAILURE;
            return SUCCESS;
        }
    }
    if (check_str(cmd, "inc ")) {
        if (inc(tmp_var, cmd))
            return FAILURE;
        return SUCCESS;
    }
    return NOT_USED;
}

// Funkce provadi prislusny prikaz (cmd) pro kazdou bunku v selekci
int commands_make(Table *table, unsigned *sel, char *cmd, Row *tmp_var)
{
    for (unsigned r = sel[R1]; r <= sel[R2]; r++) {
        for (unsigned c = sel[C1]; c <= sel[C2]; c++) {
            int fce;
            if ((fce = set_str(table, cmd, sel)) != -1) {
                if (fce)
                    return FAILURE;
            } else if ((fce = choose_function0(table, cmd, sel)) != -1) {
                if (fce)
                    return FAILURE;
            } else if ((fce = choose_function1(table, cmd, sel)) != -1) {
                if (fce)
                    return FAILURE;
            } else if (check_str(cmd, "swap")) {
                if (swap(table, &table->row[r].cell[c], cmd))
                    return FAILURE;
            } else if ((fce = tmp_var_function(table, cmd, tmp_var, sel)) != -1) {
                if (fce)
                    return FAILURE;
            }
        }
    }
    return SUCCESS;
}

// Funkce upravi velikost tabulky tak, aby sedela na selekci,
// pokud je sel[R2] > pocet radku nebo sel[C2] > pocet sloupcu
int change_table_size(Table *table, unsigned *sel)
{
    if (sel[R2] >= table->num_of_rows) {
        unsigned num_of_rows_bef = table->num_of_rows;
        if (table_resize(table, sel[R2] - table->num_of_rows + 1))
            return FAILURE;
        for (unsigned r = num_of_rows_bef; r <= sel[R2]; r++) {
            if (row_init(&table->row[r], 0))
                return FAILURE;
        }
    }
    for (unsigned r = 0; r < table->num_of_rows; r++) {
        if (sel[C2] >= table->row[r].num_of_cells) {
            unsigned num_of_cells_bef = table->row[r].num_of_cells;
            if (row_resize(&table->row[r], sel[C2] + 1))
                return FAILURE;
            if (table->num_of_cols < table->row[r].num_of_cells)
                table->num_of_cols = table->row[r].num_of_cells;
            for (unsigned c = num_of_cells_bef; c < table->row[r].num_of_cells; c++) {
                if (cell_init(&table->row[r].cell[c]))
                    return FAILURE;
            }
        }
    }
    return SUCCESS;
}

// Funkce prochazi pole prikazu a postupne je vykonava nebo meni selekci
// Vraci FAILURE, pokud se nejake funkci nepodari alokovat dostatek pameti
int commands_execute(Table *table, Row cmds, unsigned *sel)
{
    Row tmp_var =  row_ctor();
    if (row_init(&tmp_var, 0))
        return 1;
    bool failed = false;
    // postupne prochazeni prikaz za prikazem
    for(unsigned n = 0; n < cmds.num_of_cells; n++) {
        // prikaz nesmi byt prazdny
        if (cmds.cell[n].value[0] == '\0') {
            failed = true;
            break;
        }
        // pokud je prikaz selekci - zmena selekce
        if (is_selection(cmds.cell[n])) {
            if (set_selection(*table, cmds.cell[n], sel)) {
                failed = true;
                break;
            }
            if (change_table_size(table, sel)) {
                failed = true;
                break;
            }
        }
        // else provest funkce
        else {
            if (commands_make(table, sel, cmds.cell[n].value, &tmp_var)) {
                failed = true;
                break;
            }
        }
    }
    row_dtor(&tmp_var);
    if (failed)
        return FAILURE;
    return SUCCESS;
}

/***************************************************************************************/
int main(int argc, char **argv)
{
    // kontrola poctu argumentu
    if (process_arguments(argc, argv))
        return EXIT_FAILURE;
    
    // otevreni souboru
    FILE *file;
    file = fopen(argv[argc - 1], "r");
    if (file == NULL) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    
    // ziskani oddelovacu a jejich kontrola
    char *delim;
    get_delim(argc, argv, &delim);
    if (!check_delim(delim)) {
        fprintf(stderr, "\\ and \" are not valid delimiters.\n"); 
        return EXIT_FAILURE;
    }

    // vytvoreni struktury pro CMD_SEQUENCE
    Row commands = row_ctor();
    if (row_init(&commands, 0)) {
        row_dtor(&commands);
        fprintf(stderr, "Error: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
   
    // nacteni CMD_SEQUENCE do pameti
    if (commands_load(&commands, argv[argc - 2])) {
        row_dtor(&commands);
        return 1;
    }

    // vytvoreni struktury pro tabulku
    Table table = table_ctor();
    if (table_init(&table)) {
        row_dtor(&commands);
        fclose(file);
        return EXIT_FAILURE;
    }

    // nacteni tabulky do pameti
    if (table_load(file, &table, delim)) {
        row_dtor(&commands);
        table_dtor(&table);
        fclose(file);
        return EXIT_FAILURE;
    }
    fclose(file);

    // provedeni prikazu CMD_SEQUENCE
    unsigned sel[8] = {0};
    if (commands_execute(&table, commands, sel)) {
        fprintf(stderr, "Error when executing commands.\n");
        row_dtor(&commands);
        table_dtor(&table);
        return EXIT_FAILURE;
    }

    // zapis tabulky do souboru FILE
    file = fopen(argv[argc - 1], "w");
    if (file == NULL) {
        fprintf(stderr, "Error: %s\n", strerror(errno));
        row_dtor(&commands);
        table_dtor(&table);
        fclose(file);
        return EXIT_FAILURE;
    }
    table_print(file, table, delim);
    
    // uvolneni pameti alokovane pro prikazy
    row_dtor(&commands);

    // uvolneni pameti alokovane pro tabulku
    table_dtor(&table);
    
    // uzavreni souboru
    fclose(file);
    
    return EXIT_SUCCESS;
}