# Host unit tests for hardware-agnostic core logic.
add_executable(layout_test layout_test.cpp)
target_link_libraries(layout_test PRIVATE nema_core aether)

add_executable(plp_test plp_test.cpp)
target_link_libraries(plp_test PRIVATE nema_core aether)

add_executable(link_test link_test.cpp)
target_link_libraries(link_test PRIVATE nema_core aether)

add_executable(js_test js_test.cpp)
target_link_libraries(js_test PRIVATE nema_core aether)

add_executable(js_render_test js_render_test.cpp)
target_link_libraries(js_render_test PRIVATE nema_core aether)

enable_testing()
add_test(NAME layout_test COMMAND layout_test)
add_test(NAME plp_test COMMAND plp_test)
add_test(NAME link_test COMMAND link_test)
add_test(NAME js_test COMMAND js_test)
add_test(NAME js_render_test COMMAND js_render_test)

add_executable(service_test service_test.cpp)
target_link_libraries(service_test PRIVATE nema_core aether)
add_test(NAME service_test COMMAND service_test)

add_executable(js_dualimport_test js_dualimport_test.cpp)
target_link_libraries(js_dualimport_test PRIVATE nema_core aether)
add_test(NAME js_dualimport_test COMMAND js_dualimport_test)

add_executable(js_graceful_test js_graceful_test.cpp)
target_link_libraries(js_graceful_test PRIVATE nema_core aether)
add_test(NAME js_graceful_test COMMAND js_graceful_test)

add_executable(capability_test capability_test.cpp)
target_link_libraries(capability_test PRIVATE nema_core aether)
add_test(NAME capability_test COMMAND capability_test)

add_executable(cli_test cli_test.cpp)
target_link_libraries(cli_test PRIVATE nema_core aether)
add_test(NAME cli_test COMMAND cli_test)

add_executable(asset_test asset_test.cpp)
target_link_libraries(asset_test PRIVATE nema_core aether)
add_test(NAME asset_test COMMAND asset_test)

add_executable(wifi_contract_test wifi_contract_test.cpp)
target_link_libraries(wifi_contract_test PRIVATE nema_core aether)
add_test(NAME wifi_contract_test COMMAND wifi_contract_test)

add_executable(wasm_test wasm_test.cpp)
target_link_libraries(wasm_test PRIVATE nema_core aether)
add_test(NAME wasm_test COMMAND wasm_test)

add_executable(lazy_dir_loader_test lazy_dir_loader_test.cpp)
target_link_libraries(lazy_dir_loader_test PRIVATE nema_core aether)
add_test(NAME lazy_dir_loader_test COMMAND lazy_dir_loader_test)

# Wallet crypto core (Plan 94) — links the vendored trezor-crypto directly.
add_executable(wallet_crypto_test wallet_crypto_test.cpp)
target_link_libraries(wallet_crypto_test PRIVATE trezor-crypto)
add_test(NAME wallet_crypto_test COMMAND wallet_crypto_test)

# HdWallet wrapper (Plan 94) — exercises the C++ engine via nema_core.
add_executable(hd_wallet_test hd_wallet_test.cpp)
target_link_libraries(hd_wallet_test PRIVATE nema_core aether)
add_test(NAME hd_wallet_test COMMAND hd_wallet_test)

# NvsBackend (Plan 94) — encrypted-seed software custody backend.
add_executable(nvs_backend_test nvs_backend_test.cpp)
target_link_libraries(nvs_backend_test PRIVATE nema_core aether)
add_test(NAME nvs_backend_test COMMAND nvs_backend_test)

# RLP codec (Plan 94, Fase 2) — EVM transaction encoding.
add_executable(rlp_test rlp_test.cpp)
target_link_libraries(rlp_test PRIVATE nema_core aether)
add_test(NAME rlp_test COMMAND rlp_test)

# EvmChain (Plan 94, Fase 2) — EIP-155 vector + address + decode.
add_executable(evm_chain_test evm_chain_test.cpp)
target_link_libraries(evm_chain_test PRIVATE nema_core aether)
add_test(NAME evm_chain_test COMMAND evm_chain_test)

# SolanaChain (Plan 94, Fase 2) — base58 + transfer decode + sign/verify.
add_executable(solana_chain_test solana_chain_test.cpp)
target_link_libraries(solana_chain_test PRIVATE nema_core aether)
add_test(NAME solana_chain_test COMMAND solana_chain_test)

# BitcoinChain (Plan 94, Fase 2) — BIP84 P2WPKH address derivation.
add_executable(bitcoin_chain_test bitcoin_chain_test.cpp)
target_link_libraries(bitcoin_chain_test PRIVATE nema_core aether)
add_test(NAME bitcoin_chain_test COMMAND bitcoin_chain_test)

# WalletService (Plan 94, Fase 3) — orchestration across custody × chain × consent.
add_executable(wallet_service_test wallet_service_test.cpp)
target_link_libraries(wallet_service_test PRIVATE nema_core aether)
add_test(NAME wallet_service_test COMMAND wallet_service_test)

# SeBackend mode B (Plan 94, Fase 4) — SE-wrapped seed via SoftSecureElement.
add_executable(se_backend_test se_backend_test.cpp)
target_link_libraries(se_backend_test PRIVATE nema_core aether)
add_test(NAME se_backend_test COMMAND se_backend_test)

# WalletConsentService (Plan 94, Fase 5) — system-owned consent plumbing.
add_executable(wallet_consent_test wallet_consent_test.cpp)
target_link_libraries(wallet_consent_test PRIVATE nema_core aether)
add_test(NAME wallet_consent_test COMMAND wallet_consent_test)

# WalletController (Plan 94, Fase 6) — onboarding/unlock state machine.
add_executable(wallet_controller_test wallet_controller_test.cpp)
target_link_libraries(wallet_controller_test PRIVATE nema_core aether)
add_test(NAME wallet_controller_test COMMAND wallet_controller_test)

# WalletsApp (Plan 94, Fase 6) — link/smoke test for the launcher app.
add_executable(wallets_app_test wallets_app_test.cpp)
target_link_libraries(wallets_app_test PRIVATE nema_core aether)
add_test(NAME wallets_app_test COMMAND wallets_app_test)

# WalletVault (Plan 94) — multiple wallets under one PIN.
add_executable(wallet_vault_test wallet_vault_test.cpp)
target_link_libraries(wallet_vault_test PRIVATE nema_core aether)
add_test(NAME wallet_vault_test COMMAND wallet_vault_test)
