// SPDX-License-Identifier: GPL-3.0-or-later
package chat.parley

import android.app.Application
import chat.parley.data.Repository

class ParleyApp : Application() {
    val repository by lazy { Repository(this) }
}
