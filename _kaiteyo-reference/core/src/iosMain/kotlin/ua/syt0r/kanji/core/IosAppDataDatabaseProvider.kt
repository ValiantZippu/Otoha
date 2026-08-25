package ua.syt0r.kanji.core

import app.cash.sqldelight.db.SqlDriver
import app.cash.sqldelight.driver.native.NativeSqliteDriver
import co.touchlab.sqliter.DatabaseConfiguration
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Deferred
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.IO
import kotlinx.coroutines.async
import kotlinx.io.buffered
import kotlinx.io.files.Path
import kotlinx.io.files.SystemFileSystem
import org.jetbrains.compose.resources.ExperimentalResourceApi
import ua.syt0r.kanji.Res
import ua.syt0r.kanji.core.app_data.AppDataDatabaseProvider
import ua.syt0r.kanji.core.app_data.AppDataDatabaseResourceName
import ua.syt0r.kanji.core.app_data.AppDataDatabaseVersion
import ua.syt0r.kanji.core.app_data.db.AppDataDatabase
import ua.syt0r.kanji.core.logger.Logger

class IosAppDataDatabaseProvider : AppDataDatabaseProvider {

    private val coroutineScope = CoroutineScope(context = Dispatchers.IO)

    private val databasePath = Path(getPrivateAppDataDirPath())
    private val databaseName = "app_data.sqlite"

    @OptIn(ExperimentalResourceApi::class)
    override fun provideAsync(): Deferred<AppDataDatabase> = coroutineScope.async {
        SystemFileSystem.createDirectories(databasePath)
        val databaseFile = Path(databasePath, databaseName)
        if (SystemFileSystem.exists(databaseFile)) {
            SystemFileSystem.delete(databaseFile)
        }

        copyDatabaseFromResources()
        AppDataDatabase(newDriverConnection())
    }

    @OptIn(ExperimentalResourceApi::class)
    private fun newDriverConnection(): SqlDriver {
        return NativeSqliteDriver(
            schema = AppDataDatabase.Schema,
            name = databaseName,
            onConfiguration = { config ->
                config.copy(
                    version = AppDataDatabaseVersion.toInt(),
                    extendedConfig = DatabaseConfiguration.Extended(
                        basePath = getPrivateAppDataDirPath()
                    )
                )
            }
        )
    }

    @OptIn(ExperimentalResourceApi::class)
    private fun copyDatabaseFromResources() {
        val sourceUri = Res.getUri("files/$AppDataDatabaseResourceName")
        val sourcePath = Path(sourceUri.localFileUriToFilePath())
        val destinationPath = Path(databasePath, databaseName)
        Logger.d("copyDatabaseFromResources sourcePath[$sourcePath] destinationPath[$destinationPath]")

        SystemFileSystem.sink(destinationPath)
            .buffered()
            .use { sink ->
                SystemFileSystem.source(sourcePath).use { source -> sink.transferFrom(source) }
            }
    }

}

