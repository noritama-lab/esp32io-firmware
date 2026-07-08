#ifndef COMMAND_HANDLER_H
#define COMMAND_HANDLER_H

#include <ArduinoJson.h>
#include "HardwareManager.h"

class CommandHandler {
/**
 * @class CommandHandler
 * @brief JSON形式のコマンドを解析し、対応するハードウェア操作を実行します。
 * 
 * このクラスは、受信したJSON形式のコマンドを解析し、
 * それに対応するハードウェア操作（DIO、ADC、PWM、I2C、LED、センサーなど）を実行します。
 * 処理結果はJSON形式で応答として返されます。
 */
public:
    /**
     * @brief 受信したJSONコマンドを処理し、結果をJSONレスポンスとして返します。
     * @param req 受信したJSONリクエストデータ。
     * @param res 処理結果を格納するJSONドキュメント。
     */
    static void process(JsonVariantConst req, JsonDocument& res);
    /**
     * @brief エラーメッセージを含むJSONレスポンスを構築します。
     * @param res エラーメッセージを格納するJSONドキュメント。
     * @param cmd エラーが発生したコマンド名。
     * @param code エラーコード (例: "ERR_UNKNOWN", "ERR_PARAM")。
     * @param detail エラーの詳細な説明。
     */
    static void buildError(JsonDocument& res, const char* cmd, const char* code, const char* detail);

private:
    /**
     * @brief 指定されたIDが有効な範囲内にあるかを確認します。
     * @param id チェックするID。
     * @param max 許容される最大ID (排他的)。
     * @param res エラーメッセージを格納するJSONドキュメント。
     * @param cmd 実行しようとしたコマンド名。
     * @return IDが有効な範囲内であればtrue、そうでなければfalse。
     */
    static bool checkRange(int id, int max, JsonDocument& res, const char* cmd);
};
#endif