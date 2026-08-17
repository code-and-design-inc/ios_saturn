package org.uoyabause.android.phone

import org.uoyabause.android.GameInfo

enum class GameSortMode {
    NAME,
    DATE,
    RECENTLY_PLAYED,
}

sealed class GameListUiState {
    data object Initial : GameListUiState()

    data object Loading : GameListUiState()

    data class Success(
        val games: List<GameInfo>,
    ) : GameListUiState()

    data class Error(
        val message: String,
    ) : GameListUiState()

    data object Empty : GameListUiState()
}

data class GameSelectScreenState(
    val listState: GameListUiState = GameListUiState.Initial,
    val sortMode: GameSortMode = GameSortMode.NAME,
    val searchQuery: String = "",
    val selectedGame: GameInfo? = null,
    val isRefreshing: Boolean = false,
    val isSubscribed: Boolean = false,
    val gameCount: Int = 0,
    val isGameListLimited: Boolean = false,
)

sealed class GameSelectUiEvent {
    data class ShowSnackbar(
        val message: String,
    ) : GameSelectUiEvent()

    data class ShowSnackbarRes(
        val stringResId: Int,
    ) : GameSelectUiEvent()

    data class NavigateToGame(
        val gameInfo: GameInfo,
    ) : GameSelectUiEvent()

    data class ShowConfirmDelete(
        val gameInfo: GameInfo,
    ) : GameSelectUiEvent()

    data object LaunchFilePicker : GameSelectUiEvent()

    data object OpenSettings : GameSelectUiEvent()

    data object OpenBackupManager : GameSelectUiEvent()

    data object OpenAccountManager : GameSelectUiEvent()

    data class ShowProgress(
        val message: String,
    ) : GameSelectUiEvent()

    data class UpdateProgress(
        val message: String,
    ) : GameSelectUiEvent()

    data object DismissProgress : GameSelectUiEvent()

    data object SignedOut : GameSelectUiEvent()
}
