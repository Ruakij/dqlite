#include "../lib/db.h"
#include "../lib/heap.h"
#include "../lib/leader.h"
#include "../lib/logger.h"
#include "../lib/options.h"
#include "../lib/raft.h"
#include "../lib/registry.h"
#include "../lib/replication.h"
#include "../lib/runner.h"
#include "../lib/sqlite.h"
#include "../lib/vfs.h"

TEST_MODULE(leader);

#define FIXTURE              \
	FIXTURE_RAFT;        \
	FIXTURE_LOGGER;      \
	FIXTURE_VFS;         \
	FIXTURE_REPLICATION; \
	FIXTURE_OPTIONS;     \
	FIXTURE_DB;          \
	FIXTURE_LEADER;

#define SETUP              \
	SETUP_RAFT;        \
	SETUP_LOGGER;      \
	SETUP_HEAP;        \
	SETUP_SQLITE;      \
	SETUP_VFS;         \
	SETUP_REPLICATION; \
	SETUP_OPTIONS;     \
	SETUP_DB;          \
	SETUP_LEADER;

#define TEAR_DOWN              \
	TEAR_DOWN_LEADER;      \
	TEAR_DOWN_DB;          \
	TEAR_DOWN_OPTIONS;     \
	TEAR_DOWN_REPLICATION; \
	TEAR_DOWN_VFS;         \
	TEAR_DOWN_SQLITE;      \
	TEAR_DOWN_HEAP;        \
	TEAR_DOWN_LOGGER;      \
	TEAR_DOWN_RAFT;

/******************************************************************************
 *
 * leader__init
 *
 ******************************************************************************/

struct init_fixture
{
	FIXTURE;
};

TEST_SUITE(init);
TEST_SETUP(init)
{
	struct init_fixture *f = munit_malloc(sizeof *f);
	SETUP;
	return f;
}
TEST_TEAR_DOWN(init)
{
	struct init_fixture *f = data;
	TEAR_DOWN;
	free(f);
}

/* The connection is open and can be used. */
TEST_CASE(init, conn, NULL)
{
	struct init_fixture *f = data;
	sqlite3_stmt *stmt;
	int rc;
	(void)params;
	rc = sqlite3_prepare_v2(f->leader.conn, "SELECT 1", -1, &stmt, NULL);
	munit_assert_int(rc, ==, 0);
	sqlite3_finalize(stmt);
	return MUNIT_OK;
}