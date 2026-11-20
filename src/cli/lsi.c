#include <stdio.h>
#include <time.h>
#include <getopt.h>
#include <uuid/uuid.h>
#include <libsmartcols/libsmartcols.h>
#include <collectc/cc_array.h>
#include "common/entities/image.h"
#include "engine/image/lifecycle.h"
#include "cli/lsi.h"

static void nsh_cli_images_table_append_row(
    struct libscols_table *table, nsh_image_entity_t *entity)
{
    struct libscols_line *line;
    struct tm *tm;
    char id[UUID_STR_LEN];
    char created_at[64];

    line = scols_table_new_line(table, NULL);
    tm = localtime(&entity->created_at);

    uuid_unparse_lower(entity->id, id);
    strftime(created_at, sizeof(created_at), "%Y-%m-%d %H:%M:%S", tm);

    scols_line_set_data(line, 0, id);
    scols_line_set_data(line, 1, entity->name);
    scols_line_set_data(line, 2, created_at);
}

static void nsh_cli_images_table_print(struct cc_array_s *entities)
{
    struct libscols_table *table;
    nsh_image_entity_t *entity;

    table = scols_new_table();
    scols_table_set_column_separator(table, "   ");
    scols_table_new_column(table, "ID", 0, 0);
    scols_table_new_column(table, "NAME", 0, 0);
    scols_table_new_column(table, "CREATED AT", 0, 0);

    for (size_t i = 0; i < cc_array_size(entities); i++) {
        cc_array_get_at(entities, i, (void **)&entity);
        nsh_cli_images_table_append_row(table, entity);
    }

    scols_print_table(table);
    scols_unref_table(table);
}

int nsh_cli_lsi_handle()
{
    struct cc_array_s *entities;
    int err;

    err = nsh_image_list(&entities);
    if (err)
        return -1;

    nsh_cli_images_table_print(entities);
    cc_array_destroy_cb(entities, (void (*)(void *))nsh_image_entity_free);

    return 0;
}
