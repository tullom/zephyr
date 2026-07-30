# Copyright (c) 2026 Open Device Partnership and Contributors
#
# SPDX-License-Identifier: Apache-2.0

zephyr_library()

zephyr_library_sources(bq41z50.c)

zephyr_include_directories_ifdef(CONFIG_EMUL_BQ41Z50 .)
zephyr_library_sources_ifdef(CONFIG_EMUL_BQ41Z50 ./emul_bq41z50.c)
