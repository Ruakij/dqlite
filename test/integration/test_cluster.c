#include "../../src/client.h"
#include "../../src/server.h"
#include "../lib/client.h"
#include "../lib/endpoint.h"
#include "../lib/fs.h"
#include "../lib/heap.h"
#include "../lib/runner.h"
#include "../lib/server.h"
#include "../lib/sqlite.h"

/******************************************************************************
 *
 * Fixture
 *
 ******************************************************************************/

#define N_SERVERS 3
#define FIXTURE                                \
	struct test_server servers[N_SERVERS]; \
	struct client *client

#define SETUP                                                 \
	unsigned i_;                                          \
	test_heap_setup(params, user_data);                   \
	test_sqlite_setup(params);                            \
	for (i_ = 0; i_ < N_SERVERS; i_++) {                  \
		struct test_server *server = &f->servers[i_]; \
		test_server_setup(server, i_ + 1, params);    \
	}                                                     \
	test_server_network(f->servers, N_SERVERS);           \
	for (i_ = 0; i_ < N_SERVERS; i_++) {                  \
		struct test_server *server = &f->servers[i_]; \
		test_server_start(server);                    \
	}                                                     \
	SELECT(1)

#define TEAR_DOWN                                       \
	unsigned i_;                                    \
	for (i_ = 0; i_ < N_SERVERS; i_++) {            \
		test_server_tear_down(&f->servers[i_]); \
	}                                               \
	test_sqlite_tear_down();                        \
	test_heap_tear_down(data)

/* Use the client connected to the server with the given ID. */
#define SELECT(ID) f->client = test_server_client(&f->servers[ID - 1])

/******************************************************************************
 *
 * cluster
 *
 ******************************************************************************/

SUITE(cluster)

struct fixture
{
	FIXTURE;
};

static void *setUp(const MunitParameter params[], void *user_data)
{
	struct fixture *f = munit_malloc(sizeof *f);
	SETUP;
	return f;
}

static void tearDown(void *data)
{
	struct fixture *f = data;
	TEAR_DOWN;
	free(f);
}

static char* num_records[] = {
    "0", "1", "256",
    /* WAL will just have been checkpointed after 993 writes. */
    "993",
    /* Non-empty WAL, checkpointed twice, 2 snapshots taken */
    "2200", NULL
};

static MunitParameterEnum num_records_params[] = {
    { "num_records", num_records },
    { NULL, NULL },
};

/* Restart a node and check if all data is there */
TEST(cluster, restart, setUp, tearDown, 0, num_records_params)
{
	struct fixture *f = data;
	unsigned stmt_id;
	unsigned last_insert_id;
	unsigned rows_affected;
	struct rows rows;
	long n_records = strtol(munit_parameters_get(params, "num_records"), NULL, 0);
	char sql[128];

	HANDSHAKE;
	OPEN;
	PREPARE("CREATE TABLE test (n INT)", &stmt_id);
	EXEC(stmt_id, &last_insert_id, &rows_affected);

	for (int i = 0; i < n_records; ++i) {
		sprintf(sql, "INSERT INTO test(n) VALUES(%d)", i + 1);
		PREPARE(sql, &stmt_id);
		EXEC(stmt_id, &last_insert_id, &rows_affected);
	}

	struct test_server *server = &f->servers[0];
	test_server_stop(server);
	test_server_start(server);

	/* The table is visible after restart. */
	HANDSHAKE;
	OPEN;
	PREPARE("SELECT COUNT(*) from test", &stmt_id);
	QUERY(stmt_id, &rows);
	munit_assert_long(rows.next->values->integer, ==, n_records);
	clientCloseRows(&rows);
	return MUNIT_OK;
}

/* Add data to a node, add a new node and make sure data is there. */
TEST(cluster, dataOnNewNode, setUp, tearDown, 0, num_records_params)
{
	struct fixture *f = data;
	unsigned stmt_id;
	unsigned last_insert_id;
	unsigned rows_affected;
	struct rows rows;
	long n_records = strtol(munit_parameters_get(params, "num_records"), NULL, 0);
	char sql[128];
	unsigned id = 2;
	const char *address = "@2";

	HANDSHAKE;
	OPEN;
	PREPARE("CREATE TABLE test (n INT)", &stmt_id);
	EXEC(stmt_id, &last_insert_id, &rows_affected);

	for (int i = 0; i < n_records; ++i) {
		sprintf(sql, "INSERT INTO test(n) VALUES(%d)", i + 1);
		PREPARE(sql, &stmt_id);
		EXEC(stmt_id, &last_insert_id, &rows_affected);
	}

	/* Add a second voting server, this one will receive all data from the
	 * original leader. */
	ADD(id, address);
	ASSIGN(id, 1 /* voter */);

	/* Remove original server so second server becomes leader after election
	 * timeout */
	REMOVE(1);
	sleep(1);

	/* The full table is visible from the new node */
	SELECT(2);
	HANDSHAKE;
	OPEN;
	PREPARE("SELECT COUNT(*) from test", &stmt_id);
	QUERY(stmt_id, &rows);
	munit_assert_long(rows.next->values->integer, ==, n_records);
	clientCloseRows(&rows);
	return MUNIT_OK;
}
