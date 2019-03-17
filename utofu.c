#include <stdlib.h>
#include <string.h>

#include <unicode/uspoof.h>
#include <unicode/ustdio.h>
#include <unicode/ustring.h>
#include <unicode/unorm2.h>

#include <sqlite3.h>

#define BUFSZ 2048

sqlite3 *db = NULL;

void db_close(void)
{
	sqlite3_close(db);
	sqlite3_shutdown();
}

int is_newline(UChar c)
{
	/* ascii newline or U+2028 LINE SEPARATOR */
	return c == '\n' || c == 0x2028;
}

sqlite3_stmt *prepare_stmt(const char *sql)
{
	int err;
	sqlite3_stmt *ret;

	err = sqlite3_prepare_v2(db, sql, -1, &ret, NULL);
	if (err != SQLITE_OK)
	{
		fprintf(stderr,
		        "Unable to prepare query: %s\n"
				"Query is: %s\n", 
		        sqlite3_errstr(err), sql);
		exit(EXIT_FAILURE);
	}
	return ret;
}

int main(int argc, char **argv)
{
	UErrorCode status = U_ZERO_ERROR;
	UNormalizer2 *norm;
	USpoofChecker *spoof;
	UChar line[BUFSZ], normalized[BUFSZ];
	char utf8_norm[BUFSZ*2], utf8_skel[BUFSZ*2];
	const unsigned char* expected;
	sqlite3_stmt *insert_stmt, *lookup_stmt;
	UFILE *in;
	int32_t length;
	int err;

	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s history-file\n", argv[0]);
		return EXIT_FAILURE;
	}

	if ((err = sqlite3_initialize()) != SQLITE_OK)
	{
		fprintf(stderr, "Unable to initialize sqlite3: %s\n", sqlite3_errstr(err));
		return EXIT_FAILURE;
	}
	atexit(db_close);

	if ((err = sqlite3_open(argv[1], &db)) != SQLITE_OK)
	{
		fprintf(stderr, "Unable to open db \"%s\": %s\n", argv[1], sqlite3_errstr(err));
		return EXIT_FAILURE;
	}

	/* if this fails because table already exists, no prob */
	sqlite3_exec(db, 
			"CREATE TABLE corpus ("
			"  word TEXT NOT NULL,"
			"  skel TEXT PRIMARY KEY"
			");",
			NULL, NULL, NULL);

	if (!(in = u_finit(stdin, NULL, NULL)))
	{
		fputs("Error opening stdin as UFILE\n", stderr);
		return EXIT_FAILURE;
	}

	norm = (UNormalizer2 *)unorm2_getNFCInstance(&status);
	if (U_FAILURE(status)) {
		fprintf(stderr,
		        "unorm2_getNFCInstance(): %s\n",
		        u_errorName(status));
		return EXIT_FAILURE;
	}

	spoof = uspoof_open(&status);
	if (U_FAILURE(status)) {
		fprintf(stderr, "uspoof_open(): %s\n", u_errorName(status));
		return EXIT_FAILURE;
	}

	lookup_stmt = prepare_stmt(
			"SELECT word FROM corpus WHERE skel = ?;");
	insert_stmt = prepare_stmt(
			"INSERT OR IGNORE INTO corpus(word,skel) VALUES(?,?);");

	while (U_SUCCESS(status) && u_fgets(line, BUFSZ, in))
	{
		if (is_newline(line[0]) || line[0] == '\0')
			continue;
		unorm2_normalize(norm, line, -1,
		                 normalized, BUFSZ, &status);
		u_strToUTF8(utf8_norm, BUFSZ*2, &length,
		            normalized, -1, &status);
		/* cheap chomp, already have the length */
		if (is_newline(utf8_norm[length-1]))
			utf8_norm[length-1] = '\0';
		uspoof_getSkeletonUTF8(spoof, 0, utf8_norm, -1,
		                       utf8_skel, BUFSZ*2, &status);

		sqlite3_bind_text(insert_stmt, 1, utf8_norm, -1, SQLITE_STATIC);
		sqlite3_bind_text(insert_stmt, 2, utf8_skel, -1, SQLITE_STATIC);

		sqlite3_bind_text(lookup_stmt, 1, utf8_skel, -1, SQLITE_STATIC);

		/* no RETURNING clause for INSERT, need a transaction, grr */
		sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);
		sqlite3_step(insert_stmt);
		err = sqlite3_step(lookup_stmt);
		sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);

		if (err != SQLITE_ROW)
		{
			fprintf(stderr,
			        "Could not find inserted value.\n"
			        "Should not happen.\n");
			sqlite3_reset(insert_stmt);
			sqlite3_reset(lookup_stmt);
			continue;
		}

		expected = sqlite3_column_text(lookup_stmt, 0);
		if (strcmp((const char *)expected, utf8_norm) != 0)
		{
			fprintf(stderr,
			        "FAILURE: string is confusable with previous value\n"
			        "Previous: %s\n"
			        "Current : %s\n",
		   	        expected, utf8_norm);

			sqlite3_finalize(insert_stmt);
			sqlite3_finalize(lookup_stmt);
			u_fclose(in);
			exit(EXIT_FAILURE);
		}

		sqlite3_reset(insert_stmt);
		sqlite3_reset(lookup_stmt);
	}
	sqlite3_finalize(insert_stmt);
	sqlite3_finalize(lookup_stmt);

	u_fclose(in);
	return U_FAILURE(status) ? EXIT_FAILURE : EXIT_SUCCESS;
}
