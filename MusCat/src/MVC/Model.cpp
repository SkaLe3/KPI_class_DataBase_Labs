#include "Model.h"
#include <VieM/Core/Log.h>
#include <sstream>
#include <algorithm>

Model::~Model()
{
	Finish();
}

void Model::Finish()
{
	PQfinish(m_Connection);
}

bool Model::Connect(const std::string& username, const std::string& password, std::string& errorMessage)
{
	m_Connection = PQconnectdb(("dbname=MusicCatalog user=" + username + " password=" + password).c_str());

	if (PQstatus(m_Connection) == CONNECTION_BAD)
	{
		
		errorMessage = std::string("Connection to database failed:\n") +  PQerrorMessage(m_Connection);
		VM_ERROR(errorMessage);
		Finish();
		return false;
	}
	errorMessage = "";
	m_SampleDataStorage.SetConnection(m_Connection);
	m_Generator.SetConnection(m_Connection);
	return true;
}

bool Model::CreateTables(std::string& errorMessage)
{
	errorMessage = "";
	std::string error;

	error = CreateTableGenre();
	if (!error.empty())
		errorMessage = error;

	error = CreateTableLabel();
	if (!error.empty())
		errorMessage = error;

	error = CreateTableAlbum();
	if (!error.empty())
		errorMessage = error;

	error = CreateTableSong();
	if (!error.empty())
		errorMessage = error;

	error = CreateTablePerson();
	if (!error.empty())
		errorMessage = error;

	error = CreateTableArtist();
	if (!error.empty())
		errorMessage = error;

	error = CreateTableArtist_Person();
	if (!error.empty())
		errorMessage = error;

	error = CreateTableArtist_Song();
	if (!error.empty())
		errorMessage = error;

	return errorMessage.empty();
}

std::shared_ptr<TableData> Model::FetchTableData(Table table, std::vector<std::string> pkeysTitles, std::string& errorMessage)
{
	std::string query = "SELECT * FROM " + TableSpecs::GetName(table);
	query += " ORDER BY ";
	for (size_t i = 0; i < pkeysTitles.size(); ++i)
	{
		if (i > 0)
			query += ", ";
		query += pkeysTitles[i];
	}
	query += ";";
	PGresult* result = PQexec(m_Connection, query.c_str());

	std::shared_ptr<TableData> data = std::make_shared<TableData>();

	if (PQresultStatus(result) != PGRES_TUPLES_OK)
	{
		errorMessage = std::string("Select all Data Query execution failed for table:\n") + TableSpecs::GetName(table) + PQerrorMessage(m_Connection);
		VM_ERROR(errorMessage);
		PQclear(result);
		return data;
	}

	int32_t rows = PQntuples(result);
	int32_t columns = PQnfields(result);
	
	for (int row = 0; row < rows; ++row)
	{
		std::vector<std::string> rowData;
		for (int col = 0; col < columns; ++col)
		{
			rowData.emplace_back(PQgetvalue(result, row, col));
		}
		data->push_back(rowData);
	}
	PQclear(result);
	errorMessage = "";
	return data;
}


bool Model::CreateRecord(Table table, std::vector<std::string> data, std::string& errorMessage)
{
	std::string query;
	query+= "INSERT INTO " + TableSpecs::GetName(table) + " (";
	auto columns = TableSpecs::GetColumns(table);
	for (int32_t col = 0, size = columns.size(); col < size; col++)
	{
		if (columns[col].Type == ColumnType::Serial)
			continue;
		query += columns[col].Name;
		if (size - col != 1)
			query += ",";
	}
	query += ") VALUES (";
	for (int32_t col = 0, size = data.size(); col < size; col++)
	{
		if (columns[col].Type == ColumnType::Serial)
			continue;
		if (data[col].empty())
		{
			query += "NULL";
		}
		else 
		{
			query += "'" + data[col] + "'";
		}
		if (size - col != 1)
			query += ",";
	}
	query += ")";


	PGresult* result = PQexec(m_Connection, query.c_str());

	if (PQresultStatus(result) != PGRES_COMMAND_OK) {
		
		errorMessage = std::string("Insert Query execution failed for table: ") + TableSpecs::GetName(table) + "\n" + PQerrorMessage(m_Connection);
		VM_ERROR(errorMessage);
		VM_ERROR("Query text was: ", query, "\n");
		PQclear(result);
		return false;
	}
	PQclear(result);
	errorMessage = "";
	return true;
}

bool Model::UpdateRecord(Table table, std::vector<std::string> data, std::vector<Column> keys, std::vector<std::string> oldData, std::string& errorMessage)
{
	std::string query = "UPDATE " + TableSpecs::GetName(table) + " SET ";

	std::vector<Column> columns = TableSpecs::GetColumns(table);


	for (size_t i = 0; i < columns.size(); ++i)
	{

		if (columns[i].Type == ColumnType::Serial)
			continue;
		query += columns[i].Name + " = ";
		if (data[i].empty())
			query += "NULL";
		else
			query += "'" + data[i] + "'";
		if (columns.size() - i != 1)
			query += ", ";

	}

	query += " WHERE ";

	for (size_t i = 0; i < keys.size(); ++i)
	{
		if (i > 0) 
			query += " AND ";

		query += keys[i].Name + " = ";
		query += "'" + oldData[i] + "'";
	}
	query += ";";
	
	PGresult* res = PQexec(m_Connection, query.c_str());

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		errorMessage = std::string("Update Record Query execution failed for table: ") + TableSpecs::GetName(table) + "\n" + PQerrorMessage(m_Connection);
		VM_ERROR(errorMessage);
		VM_ERROR("Query text was: ", query, "\n");
		PQclear(res);
		return false;
	}

	PQclear(res);
	return true;
}

bool Model::DeleteRecord(Table table, std::vector<Column> pkeyColumns, std::vector<std::string> pkeysData, std::string& errorMessage)
{
	std::string query = "DELETE FROM " + TableSpecs::GetName(table) + " WHERE ";

	for (size_t i = 0; i < pkeyColumns.size(); ++i)
	{
		if (i > 0)
			query += " AND ";

		query += pkeyColumns[i].Name + " = ";
		query += "'" + pkeysData[i] + "'";
	}
	query += ";";

	PGresult* res = PQexec(m_Connection, query.c_str());

	if (PQresultStatus(res) != PGRES_COMMAND_OK)
	{
		errorMessage = std::string("Delete Record Query execution failed for table: ") + TableSpecs::GetName(table) + "\n" + PQerrorMessage(m_Connection);
		VM_ERROR(errorMessage);
		VM_ERROR("Query text was: ", query, "\n");
		PQclear(res);
		return false;
	}

	PQclear(res);
	return true;
}

std::vector<std::string> Model::GetRecordIfExists(Table table, std::vector<Column> columns, std::vector<std::string> data, std::string& errorMessage)
{
	std::string query = "SELECT * FROM " + TableSpecs::GetName(table) + " WHERE ";
	for (size_t i = 0; i < columns.size(); ++i) {
		if (i > 0) {
			query += " AND ";
		}
		query += columns[i].Name + " = $" + std::to_string(i + 1);
	}

	// Prepare the statement and bind the parameters.
	const char* paramValues[10];
	for (size_t i = 0; i < data.size(); ++i) {
		paramValues[i] = data[i].empty() ? "NULL" : data[i].c_str();
	}

	PGresult* res = PQexecParams(m_Connection, query.c_str(), data.size(), nullptr, paramValues, nullptr, nullptr, 0);

	std::vector<std::string> recordData;

	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		errorMessage = std::string("Find Record Query execution failed for table: ") + TableSpecs::GetName(table) + "\n" + PQerrorMessage(m_Connection);
		VM_ERROR(errorMessage);
		VM_ERROR("Query text was: ", query, "\n");
		PQclear(res);
		return recordData;
	}

	int32_t dataColumns = PQnfields(res);

	for (int col = 0; col < dataColumns; ++col)
	{
		recordData.emplace_back(PQgetvalue(res, 0, col));
	}

	PQclear(res);
	errorMessage = "No such record exists";
	return recordData;
}

std::vector<std::string> Model::GetPKeyColumnTitles(Table table, std::string& errorMessage)
{
	std::string tableName = TableSpecs::GetName(table);

	std::transform(tableName.begin(), tableName.end(), tableName.begin(),
		[](unsigned char c) { return std::tolower(c); }
	);
	std::string query = 
		"SELECT column_name "
		"FROM information_schema.key_column_usage "
		"WHERE constraint_name = ( "
		"	SELECT constraint_name "
		"	FROM information_schema.table_constraints "
		"	WHERE table_name = '" + tableName + 
		"'	AND constraint_type = 'PRIMARY KEY' "
		"	) "
		"ORDER BY ordinal_position; ";

	PGresult* res = PQexec(m_Connection, query.c_str());
	std::vector<std::string> data;
	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		errorMessage = std::string("Get Primary Keys Query execution failed for table: ") + TableSpecs::GetName(table) + "\n" + PQerrorMessage(m_Connection);
		VM_ERROR(errorMessage);
		VM_ERROR("Query text was: ", query, "\n");
		PQclear(res);
		return data;
	}

	
	int32_t rows = PQntuples(res);

	for (int row = 0; row < rows; ++row)
	{
		data.emplace_back(PQgetvalue(res, row, 0));	
	}
	PQclear(res);
	errorMessage = "";
	return data;

}

bool Model::GenerateData(Table table,int32_t rowCount, std::string& errorMessage)
{
	bool success = true;
	switch (table)
	{
	case Table::Genre:
		success &= m_Generator.GenerateGenres(m_SampleDataStorage.GetGenres(), errorMessage);
		break;
	case Table::Label:
		success &= m_Generator.GenerateLabels(m_SampleDataStorage.GetLabels(), m_SampleDataStorage.GetLocations(), errorMessage);
		break;
	case Table::Album:
		success &= m_Generator.GenerateAlbums(rowCount, errorMessage);
		break;
	case Table::Song:
	{
		bool notempty = CheckTableMinRecords(Table::Genre, 1);
		if (notempty)
			success &= m_Generator.GenerateSong(rowCount, errorMessage);
		else {
			success = false;
			errorMessage += "\n\nYou need at least 1 genre to exist to generate songs";
		}
		break;
	}
	case Table::Person:
		success &= m_Generator.GeneratePerson(rowCount, errorMessage);
		break;
	case Table::Artist:
		success &= m_Generator.GenerateArtist(rowCount, errorMessage);
		break;
	case Table::Artist_Person:
	{

		if (CheckTableMinRecords(Table::Artist, 1) && CheckTableMinRecords(Table::Person, 1))
			success &= m_Generator.GenerateArtist_Person(rowCount, errorMessage);
		else {
			success = false;
			errorMessage += "\n\nYou need at least 1 artist and 1 person to exist to generate artist-person relation";
		}
		break;
	}
	case Table::Artist_Song:
	{
		if (CheckTableMinRecords(Table::Artist, 1) && CheckTableMinRecords(Table::Song, 1))
			success &= m_Generator.GenerateArtist_Song(rowCount, errorMessage);
		else {
			success = false;
			errorMessage += "\n\nYou need at least 1 artist and 1 song to exist to generate artist-song relation";
		}
		break;
	}

	}
	return success;
}

bool Model::CheckTableMinRecords(Table table, int32_t count)
{
	std::string tableName = TableSpecs::GetName(table);
	std::string query = "SELECT count(*) FROM " + tableName + ";";

	PGresult* result = PQexec(m_Connection, query.c_str());

	if (PQresultStatus(result) != PGRES_TUPLES_OK)
	{
		std::string errorMessage = std::string("Failed to check table emptiness:  ") + TableSpecs::GetName(table) +  "\n" + PQerrorMessage(m_Connection);
		VM_ERROR(errorMessage);
		PQclear(result);
		return false;
	}

	if (atoi(PQgetvalue(result, 0, 0)) >= count)
		return true;
	return false;

}

bool Model::LoadTestDataSamples(std::string& errorMessage)
{
	errorMessage = "Failed to load test data samples";
	return m_SampleDataStorage.LoadFiles();
}

bool Model::CreateAuxiliaryTablesForTestData(std::string& errorMessage)
{
	errorMessage = "Failed to create auxiliary tables";
	return m_SampleDataStorage.CreateTables();
}

std::string Model::CreateTableGenre()
{
	const char* createTableSQL = 
		"CREATE TABLE IF NOT EXISTS genre ("
		"genre_id serial NOT NULL,"
		"name character varying(64) NOT NULL,"
		"CONSTRAINT genre_id_pk PRIMARY KEY (genre_id));";

	PGresult* res = PQexec(m_Connection, createTableSQL);

	return CheckCreateResult(res, PGRES_COMMAND_OK , "genre");
}

std::string Model::CreateTableLabel()
{
	const char* createTableSQL = 
		"CREATE TABLE IF NOT EXISTS label ("
		"label_id serial NOT NULL,"
		"name character varying(64) NOT NULL,"
		"location character varying(64) NOT NULL,"
		"founded_date date NOT NULL,"
		"CONSTRAINT label_id_pk PRIMARY KEY (label_id));";

	PGresult* res = PQexec(m_Connection, createTableSQL);

	return CheckCreateResult(res, PGRES_COMMAND_OK, "label");
}

std::string Model::CreateTableAlbum()
{
	const char* createTableSQL =
		"CREATE TABLE IF NOT EXISTS album ("
		"album_id serial NOT NULL,"
		"title character varying(64) NOT NULL,"
		"CONSTRAINT album_id_pk PRIMARY KEY (album_id))";

	PGresult* res = PQexec(m_Connection, createTableSQL);

	return CheckCreateResult(res, PGRES_COMMAND_OK, "album");
}

std::string Model::CreateTableSong()
{
	const char* createTableSQL = 
		"CREATE TABLE IF NOT EXISTS song ("
		"song_id serial NOT NULL,"
		"title character varying(64) NOT NULL,"
		"release_date date NOT NULL,"
		"label_id integer,"
		"genre_id integer NOT NULL,"
		"album_id integer,"
		"CONSTRAINT song_id_pk PRIMARY KEY (song_id), "
		"CONSTRAINT song_album_id_fk FOREIGN KEY (album_id) "
			"REFERENCES public.album(album_id) ON DELETE CASCADE, "
		"CONSTRAINT song_genre_id_fk FOREIGN KEY (genre_id) "
			"REFERENCES public.genre(genre_id) ON DELETE RESTRICT, "
		"CONSTRAINT song_label_id_fk FOREIGN KEY (label_id) "
			"REFERENCES public.label(label_id) ON DELETE CASCADE);";

	PGresult* res = PQexec(m_Connection, createTableSQL);

	return CheckCreateResult(res, PGRES_COMMAND_OK, "song");
}



std::string Model::CreateTablePerson()
{
	const char* createTableSQL =
		"CREATE TABLE IF NOT EXISTS person ("
		"person_id serial NOT NULL,"
		"name character varying(64) NOT NULL,"
		"birth_date date NOT NULL,"
		"death_date date,"
		"CONSTRAINT person_id_pk PRIMARY KEY (person_id))";

	PGresult* res = PQexec(m_Connection, createTableSQL);

	return CheckCreateResult(res, PGRES_COMMAND_OK, "person");
}

std::string Model::CreateTableArtist()
{
	const char* createTableSQL =
		"CREATE TABLE IF NOT EXISTS artist ("
		"artist_id serial NOT NULL,"
		"title character varying(64) NOT NULL,"
		"description text,"
		"founded_date date NOT NULL,"
		"closed_date date,"
		"label_id integer,"
		"CONSTRAINT artist_id_pk PRIMARY KEY (artist_id),"
		"CONSTRAINT artist_label_id_fk FOREIGN KEY (label_id)"
			"REFERENCES public.label (label_id) ON DELETE SET NULL);";

	PGresult* res = PQexec(m_Connection, createTableSQL);

	return CheckCreateResult(res, PGRES_COMMAND_OK, "artist");
}

std::string Model::CreateTableArtist_Person()
{
	const char* createTableSQL =
		"CREATE TABLE IF NOT EXISTS artist_person ("
		"artist_id integer NOT NULL,"
		"person_id integer NOT NULL,"
		"CONSTRAINT artist_person_pk PRIMARY KEY (artist_id, person_id),"
		"CONSTRAINT artist_person_artist_id_fk FOREIGN KEY (artist_id)"
			"REFERENCES public.artist (artist_id) ON DELETE CASCADE,"
		"CONSTRAINT artist_person_person_id_fk FOREIGN KEY (person_id)"
			"REFERENCES public.person (person_id) ON DELETE CASCADE);";


	PGresult* res = PQexec(m_Connection, createTableSQL);

	return CheckCreateResult(res, PGRES_COMMAND_OK, "artist_person");
}

std::string Model::CreateTableArtist_Song()
{
	const char* createTableSQL =
		"CREATE TABLE IF NOT EXISTS artist_song ("
		"artist_id integer NOT NULL,"
		"song_id integer NOT NULL,"
		"CONSTRAINT artist_song_pk PRIMARY KEY (artist_id, song_id),"
		"CONSTRAINT artist_song_artist_id_fk FOREIGN KEY (artist_id)"
			"REFERENCES public.artist (artist_id) ON DELETE CASCADE,"
		"CONSTRAINT artist_song_song_id_fk FOREIGN KEY (song_id)"
			"REFERENCES public.song (song_id) ON DELETE CASCADE);";


	PGresult* res = PQexec(m_Connection, createTableSQL);

	return CheckCreateResult(res, PGRES_COMMAND_OK, "artist_song");
}

std::string Model::CheckCreateResult(PGresult* res, ExecStatusType status, const std::string& text)
{
	if (PQresultStatus(res) != status)
	{
		std::string errorMessage = std::string("Query execution failed for table: ") + text + "\n" + PQerrorMessage(m_Connection);
		VM_ERROR(errorMessage);
		PQclear(res);
		Finish();
		return errorMessage;
	}
	PQclear(res);
	return std::string();
}

