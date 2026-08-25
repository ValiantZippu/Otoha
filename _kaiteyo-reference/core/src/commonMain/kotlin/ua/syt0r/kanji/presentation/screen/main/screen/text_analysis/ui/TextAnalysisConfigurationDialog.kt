package ua.syt0r.kanji.presentation.screen.main.screen.text_analysis.ui

import androidx.compose.foundation.background
import androidx.compose.foundation.clickable
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.outlined.Check
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.Text
import androidx.compose.material3.TextButton
import androidx.compose.runtime.Composable
import androidx.compose.ui.Alignment
import androidx.compose.ui.Modifier
import androidx.compose.ui.draw.clip
import org.jetbrains.compose.resources.stringResource
import ua.syt0r.kanji.Res
import ua.syt0r.kanji.dialog_cancel
import ua.syt0r.kanji.presentation.common.MultiplatformDialog
import ua.syt0r.kanji.presentation.common.theme.Dimens
import ua.syt0r.kanji.text_analysis_configuration_analysis_provider
import ua.syt0r.kanji.text_analysis_configuration_title
import ua.syt0r.kanji.text_analysis_configuration_translation_provider
import ua.syt0r.kanji.text_analysis_ichiran_description

@Composable
fun TextAnalysisConfigurationDialog(onDismissRequest: () -> Unit) {
    val categoryRow: @Composable (String) -> Unit = { title ->
        Text(
            text = title,
            style = MaterialTheme.typography.labelLarge,
            modifier = Modifier.padding(vertical = Dimens.SpacingBig)
        )
    }

    val clickableRow: @Composable (String, String?) -> Unit = { title, subtitle ->

        Row(
            horizontalArrangement = Arrangement.spacedBy(Dimens.SpacingMid),
            verticalAlignment = Alignment.CenterVertically,
            modifier = Modifier
                .fillMaxWidth()
                .clip(MaterialTheme.shapes.small)
                .background(MaterialTheme.colorScheme.surfaceDim)
                .clickable { }
                .padding(horizontal = Dimens.SpacingBig, vertical = Dimens.SpacingMid)
        ) {

            Icon(Icons.Outlined.Check, null)

            Column {
                Text(
                    text = title,
                    style = MaterialTheme.typography.labelMedium
                )
                subtitle?.also {
                    Text(
                        text = it,
                        style = MaterialTheme.typography.labelSmall
                    )
                }
            }

        }

    }

    MultiplatformDialog(
        onDismissRequest = onDismissRequest,
        title = { Text(stringResource(Res.string.text_analysis_configuration_title)) },
        content = {
            categoryRow(stringResource(Res.string.text_analysis_configuration_analysis_provider))
            clickableRow("Ichiran", stringResource(Res.string.text_analysis_ichiran_description))
            categoryRow(stringResource(Res.string.text_analysis_configuration_translation_provider))
            clickableRow("Gemini 2.0 Flash", null)
        },
        buttons = {
            TextButton(onDismissRequest) {
                Text(stringResource(Res.string.dialog_cancel))
            }
        }
    )

}