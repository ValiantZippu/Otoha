package ua.syt0r.kanji.presentation.common

import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.CheckCircle
import androidx.compose.material.icons.outlined.Circle
import androidx.compose.material3.Icon
import androidx.compose.material3.MaterialTheme
import androidx.compose.runtime.Composable
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.vector.ImageVector

@Composable
fun AppCheckBox(checked: Boolean) {
    val icon: ImageVector
    val color: Color

    when {
        checked -> {
            icon = Icons.Default.CheckCircle
            color = MaterialTheme.colorScheme.primary
        }

        else -> {
            icon = Icons.Outlined.Circle
            color = MaterialTheme.colorScheme.surfaceVariant
        }
    }

    Icon(imageVector = icon, contentDescription = null, tint = color)
}