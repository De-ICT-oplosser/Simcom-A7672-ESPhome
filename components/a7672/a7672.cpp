#include "a7672.h" // Renamed include
#include "esphome/core/log.h"
#include <cstring>

namespace esphome {
namespace a7672 { // Renamed namespace

static const char *const TAG = "a7672"; // Renamed logging tag

const char ASCII_CR = 0x0D;
const char ASCII_LF = 0x0A;

void A7672Component::update() { // Renamed class
  if (this->watch_dog_++ == 2) {
    this->state_ = STATE_INIT;
    this->write(26); // CTRL+Z, likely still needed
  }

  if (this->expect_ack_)
    return;

  // Logic inside update() assumes SIM800L AT command flow, may need adjustments for A7672
  if (state_ == STATE_INIT) {
    if (this->registered_ && this->send_pending_) {
      this->send_cmd_("AT+CSCS=\"GSM\""); // Check if A7672 needs different charset handling
      this->state_ = STATE_SENDING_SMS_1;
    } else if (this->registered_ && this->dial_pending_) {
      this->send_cmd_("AT+CSCS=\"GSM\"");
      this->state_ = STATE_DIALING1;
    } else if (this->registered_ && this->connect_pending_) {
      this->connect_pending_ = false;
      ESP_LOGI(TAG, "Connecting...");
      this->send_cmd_("ATA");
      this->state_ = STATE_ATA_SENT;
    } else if (this->registered_ && this->send_ussd_pending_) {
      this->send_cmd_("AT+CSCS=\"GSM\"");
      this->state_ = STATE_SEND_USSD1;
    } else if (this->registered_ && this->disconnect_pending_) {
      this->disconnect_pending_ = false;
      ESP_LOGI(TAG, "Disconnecting...");
      this->send_cmd_("ATH");
    } else if (this->registered_ && this->call_state_ != 6) {
      send_cmd_("AT+CLCC"); // Check if A7672 uses same call status command
      this->state_ = STATE_CHECK_CALL;
      return;
    } else {
      this->send_cmd_("AT"); // Basic attention command, usually safe
      this->state_ = STATE_SETUP_CMGF;
    }
    this->expect_ack_ = true;
  } else if (state_ == STATE_RECEIVED_SMS) {
    // Serial Buffer should have flushed.
    // Send cmd to delete received sms
    char delete_cmd[20];
    sprintf(delete_cmd, "AT+CMGD=%d", this->parse_index_); // Check if A7672 uses same delete command
    this->send_cmd_(delete_cmd);
    this->state_ = STATE_CHECK_SMS;
    this->expect_ack_ = true;
  }
}

void A7672Component::send_cmd_(const std::string &message) { // Renamed class
  ESP_LOGV(TAG, "S: %s - %d", message.c_str(), this->state_);
  this->watch_dog_ = 0;
  this->write_str(message.c_str());
  this->write_byte(ASCII_CR);
  this->write_byte(ASCII_LF);
}

void A7672Component::parse_cmd_(std::string message) { // Renamed class
  if (message.empty())
    return;

  ESP_LOGV(TAG, "R: %s - %d", message.c_str(), this->state_);

  // ***** CRITICAL AREA *****
  // This entire function heavily relies on parsing specific responses
  // from the SIM800L. You MUST verify these responses against the
  // A7672 AT command manual and adjust the parsing logic accordingly.
  // Examples: RING, NO CARRIER, +CMTI:, +CUSD:, +CREG:, +CSQ:, +CMGL:, +CLCC:
  // The format and parameters of these responses might differ.

  if (this->state_ != STATE_RECEIVE_SMS) {
    if (message == "RING") {
      this->state_ = STATE_PARSE_CLIP;
      this->expect_ack_ = false;
    } else if (message == "NO CARRIER") {
      if (this->call_state_ != 6) {
        this->call_state_ = 6;
        this->call_disconnected_callback_.call();
      }
    }
  }

  bool ok = message == "OK";
  if (this->expect_ack_) {
    this->expect_ack_ = false;
    if (!ok) {
      if (this->state_ == STATE_SETUP_CMGF && message == "AT") {
        this->state_ = STATE_DISABLE_ECHO;
        this->expect_ack_ = true;
      } else {
        ESP_LOGW(TAG, "Not ack. %d %s", this->state_, message.c_str());
        this->state_ = STATE_IDLE;
        return;
      }
    }
  } else if (ok && (this->state_ != STATE_PARSE_SMS_RESPONSE && this->state_ != STATE_CHECK_CALL &&
                    this->state_ != STATE_RECEIVE_SMS && this->state_ != STATE_DIALING2)) {
    ESP_LOGW(TAG, "Received unexpected OK. Ignoring");
    return;
  }

  switch (this->state_) {
    case STATE_INIT: {
      bool message_available = message.compare(0, 6, "+CMTI:") == 0; // Verify A7672 SMS indication format
      if (!message_available) {
        if (message == "RING") {
          this->state_ = STATE_PARSE_CLIP;
        } else if (message == "NO CARRIER") {
          if (this->call_state_ != 6) {
            this->call_state_ = 6;
            this->call_disconnected_callback_.call();
          }
        } else if (message.compare(0, 6, "+CUSD:") == 0) { // Verify A7672 USSD format
          this->state_ = STATE_CHECK_USSD;
        }
        break;
      }
    }
    case STATE_CHECK_SMS:
      // send_cmd_("AT+CMGL=\"ALL\""); // Verify A7672 list SMS command
      this->state_ = STATE_PARSE_SMS_RESPONSE;
      this->parse_index_ = 0;
      break;
    case STATE_DISABLE_ECHO:
      send_cmd_("ATE0");
      this->state_ = STATE_SETUP_CMGF;
      this->expect_ack_ = true;
      break;
    case STATE_SETUP_CMGF:
      send_cmd_("AT+CMGF=1"); // Text mode SMS, likely same
      this->state_ = STATE_SETUP_CLIP;
      this->expect_ack_ = true;
      break;
    case STATE_SETUP_CLIP:
      send_cmd_("AT+CLIP=1"); // Caller Line Identification, likely same
      this->state_ = STATE_CREG;
      this->expect_ack_ = true;
      break;
    case STATE_SETUP_USSD:
      send_cmd_("AT+CUSD=1"); // Enable USSD result presentation, likely same
      this->state_ = STATE_CREG;
      this->expect_ack_ = true;
      break;
    case STATE_SEND_USSD1:
      this->send_cmd_("AT+CUSD=1, \"" + this->ussd_ + "\""); // Verify USSD send format
      this->state_ = STATE_SEND_USSD2;
      this->expect_ack_ = true;
      break;
    case STATE_SEND_USSD2:
      ESP_LOGD(TAG, "SendUssd2: '%s'", message.c_str());
      if (message == "OK") {
        ESP_LOGD(TAG, "Dialing ussd code: '%s' done.", this->ussd_.c_str());
        this->state_ = STATE_CHECK_USSD;
        this->send_ussd_pending_ = false;
      } else {
        this->set_registered_(false);
        this->state_ = STATE_INIT;
        this->send_cmd_("AT+CMEE=2"); // Enable detailed error reporting
        this->write(26);
      }
      break;
    case STATE_CHECK_USSD:
      ESP_LOGD(TAG, "Check ussd code: '%s'", message.c_str());
      if (message.compare(0, 6, "+CUSD:") == 0) { // Verify USSD response format
        this->state_ = STATE_RECEIVED_USSD;
        this->ussd_ = "";
        size_t start = 10; // Adjust indices based on A7672 response format
        size_t end = message.find_last_of(',');
        if (end > start) {
          this->ussd_ = message.substr(start + 1, end - start - 2);
          this->ussd_received_callback_.call(this->ussd_);
        }
      }
      if (message == "OK")
        this->state_ = STATE_INIT;
      break;
    case STATE_CREG:
      send_cmd_("AT+CREG?"); // Network registration status, likely same
      this->state_ = STATE_CREG_WAIT;
      break;
    case STATE_CREG_WAIT: {
      // Verify A7672 CREG response format and values (e.g., '1' or '5' for registered)
      bool registered = message.compare(0, 6, "+CREG:") == 0 && (message[9] == '1' || message[9] == '5');
      if (registered) {
        if (!this->registered_) {
          ESP_LOGD(TAG, "Registered OK");
        }
        this->state_ = STATE_CSQ;
        this->expect_ack_ = true;
      } else {
        ESP_LOGW(TAG, "Registration Fail");
        if (message.length() > 7 && message[7] == '0') { // Verify CREG status index
          send_cmd_("AT+CREG=1");
          this->expect_ack_ = true;
          this->state_ = STATE_SETUP_CMGF; // Or maybe STATE_CREG again
        } else {
          this->state_ = STATE_INIT;
        }
      }
      set_registered_(registered);
      break;
    }
    case STATE_CSQ:
      send_cmd_("AT+CSQ"); // Signal quality, likely same
      this->state_ = STATE_CSQ_RESPONSE;
      break;
    case STATE_CSQ_RESPONSE:
      // Verify A7672 CSQ response format
      if (message.compare(0, 5, "+CSQ:") == 0) {
        size_t comma = message.find(',', 6);
        if (comma != std::string::npos && comma != 6) { // Check comma position validity
          int rssi_val = parse_number<int>(message.substr(6, comma - 6)).value_or(99); // raw RSSI value
          // SIM800L reports 0..31 or 99. A7672 might be similar.
          // Conversion to dBm: If rssi is 0-31, dBm = -113 + 2 * rssi. 99 means not known/detectable.
          int rssi_dbm = (rssi_val >= 0 && rssi_val <= 31) ? (-113 + 2 * rssi_val) : -115; // Use -115 or similar for unknown

#ifdef USE_SENSOR
          if (this->rssi_sensor_ != nullptr) {
            this->rssi_sensor_->publish_state(rssi_dbm);
          } else {
            ESP_LOGD(TAG, "RSSI: %d (%d dBm)", rssi_val, rssi_dbm);
          }
#else
          ESP_LOGD(TAG, "RSSI: %d (%d dBm)", rssi_val, rssi_dbm);
#endif
        }
      }
      this->expect_ack_ = true;
      this->state_ = STATE_CHECK_SMS;
      break;
    case STATE_PARSE_SMS_RESPONSE:
      // Verify A7672 +CMGL (list SMS) response format and indices
      if (message.compare(0, 6, "+CMGL:") == 0 && this->parse_index_ == 0) {
        size_t start = 7;
        size_t end = message.find(',', start);
        uint8_t item = 0;
        while (end != std::string::npos && end != start) {
          item++;
          if (item == 1) { // Index
            this->parse_index_ = parse_number<uint8_t>(message.substr(start, end - start)).value_or(0);
          }
          if (item == 3) { // Sender number
            this->sender_ = message.substr(start + 1, end - start - 2); // Remove quotes
            this->message_.clear();
            break;
          }
          start = end + 1;
          end = message.find(',', start);
        }

        if (item < 2) { // Need at least index and status
          ESP_LOGW(TAG, "Invalid +CMGL response: %s", message.c_str());
          this->state_ = STATE_INIT; // Reset state on parse error
          return;
        }
        this->state_ = STATE_RECEIVE_SMS;
      }
      if (ok) {
        send_cmd_("AT+CLCC"); // Check call status after checking messages
        this->state_ = STATE_CHECK_CALL;
      }
      break;
    case STATE_CHECK_CALL:
      // Verify A7672 +CLCC (list current calls) response format and indices
      if (message.compare(0, 6, "+CLCC:") == 0 && this->parse_index_ == 0) {
        this->expect_ack_ = true;
        size_t start = 7;
        size_t end = message.find(',', start);
        uint8_t item = 0;
        while (end != std::string::npos && end != start) {
          item++;
          if (item == 3) { // Call state (0=active, 1=held, ..., 6=disconnected)
            uint8_t current_call_state = parse_number<uint8_t>(message.substr(start, end - start)).value_or(6);
            if (current_call_state != this->call_state_) {
              ESP_LOGD(TAG, "Call state is now: %d", current_call_state);
              if (current_call_state == 0 && this->call_state_ != 0) // Trigger only on transition to active
                this->call_connected_callback_.call();
              if (current_call_state == 6 && this->call_state_ != 6) // Trigger only on transition to disconnected
                this->call_disconnected_callback_.call();
            }
            this->call_state_ = current_call_state;
            break;
          }
          start = end + 1;
          end = message.find(',', start);
        }

        if (item < 3) { // Need at least index, dir, state
           ESP_LOGW(TAG, "Invalid +CLCC response: %s", message.c_str());
           // Don't reset state here, wait for OK
        }
      } else if (ok) {
        // If OK received and no +CLCC lines came before it, means no active calls.
        if (this->call_state_ != 6) {
          this->call_state_ = 6;
          this->call_disconnected_callback_.call();
        }
        this->state_ = STATE_INIT; // Finished checking calls, go back to init/idle
      }
      break;
    case STATE_RECEIVE_SMS:
      // This part assumes the message body follows the +CMGL line
      if (ok || message.compare(0, 6, "+CMGL:") == 0) { // End of message body or start of next message
        ESP_LOGD(TAG, "Received SMS from: %s", this->sender_.c_str());
        ESP_LOGD(TAG, "%s", this->message_.c_str());
        this->sms_received_callback_.call(this->message_, this->sender_);
        this->state_ = STATE_RECEIVED_SMS; // Transition to state where we delete the SMS
        // If it was another +CMGL, process it next loop
        if (message.compare(0, 6, "+CMGL:") == 0) {
             this->parse_cmd_(message); // Re-parse the new +CMGL line immediately
        }
      } else {
        if (!this->message_.empty())
          this->message_ += "\n";
        this->message_ += message;
      }
      break;
    case STATE_RECEIVED_SMS: // SMS successfully received and callback called
    case STATE_RECEIVED_USSD: // USSD successfully received and callback called
      // Let the buffer flush. Next poll (via update()) will request deletion or next check.
      this->state_ = STATE_INIT; // Go back to init state for next cycle
      break;
    case STATE_SENDING_SMS_1:
      this->send_cmd_("AT+CMGS=\"" + this->recipient_ + "\""); // Send SMS command, likely same
      this->state_ = STATE_SENDING_SMS_2;
      // Don't set expect_ack_ here, wait for '>' prompt
      break;
    case STATE_SENDING_SMS_2:
      if (message == ">") { // Prompt for SMS body, likely same
        ESP_LOGI(TAG, "Sending to %s message: '%s'", this->recipient_.c_str(), this->outgoing_message_.c_str());
        this->write_str(this->outgoing_message_.c_str());
        this->write(26); // CTRL+Z to send, likely same
        this->state_ = STATE_SENDING_SMS_3;
        // Don't set expect_ack_ here, wait for +CMGS response
      } else { // Did not get prompt
        ESP_LOGE(TAG, "Failed to get SMS prompt '>'");
        set_registered_(false); // Assume connection issue if prompt fails
        this->state_ = STATE_INIT;
        this->send_cmd_("AT+CMEE=2");
        this->write(26); // Send CTRL+Z just in case
      }
      break;
      case STATE_SENDING_SMS_3:
      // We have sent the SMS body and CTRL+Z. Now we expect +CMGS: <mr>
      // The modem will also send an OK after +CMGS: <mr>
      
      { // Added braces for local variable scope
        std::string trimmed_message = message;
        // Trim leading/trailing whitespace from the message
        trimmed_message.erase(0, trimmed_message.find_first_not_of(" \t\r\n"));
        trimmed_message.erase(trimmed_message.find_last_not_of(" \t\r\n") + 1);

        if (trimmed_message.empty()) { // If the line was only whitespace (like the stray space)
          ESP_LOGV(TAG, "Ignored empty/whitespace line in STATE_SENDING_SMS_3, waiting for actual response.");
          // Stay in STATE_SENDING_SMS_3 to process the next line
        } else if (trimmed_message.compare(0, 6, "+CMGS:") == 0) {
          ESP_LOGD(TAG, "SMS Sent Confirmed (+CMGS): %s", trimmed_message.c_str());
          this->send_pending_ = false; // Mark SMS as sent
          // After +CMGS, the modem will send an OK.
          this->state_ = STATE_INIT;   // Transition to STATE_INIT (or STATE_CHECK_SMS).
                                       // The expect_ack_ logic will handle the upcoming OK.
          this->expect_ack_ = true;    // Set to expect the final "OK" that confirms the +CMGS sequence.
        } else if (trimmed_message.find("ERROR") != std::string::npos) {
          ESP_LOGE(TAG, "SMS Send Failed with explicit ERROR: %s", trimmed_message.c_str());
          this->send_pending_ = false;
          this->state_ = STATE_INIT;
        } else {
          // Some other unexpected non-empty line. Log it but stay in this state.
          ESP_LOGW(TAG, "Unexpected non-empty line in STATE_SENDING_SMS_3: '%s'. Waiting for +CMGS or ERROR.", trimmed_message.c_str());
          // Stay in STATE_SENDING_SMS_3 to process the next incoming line from the modem.
          // The watchdog (watch_dog_++) will eventually reset if the modem gets truly stuck.
        }
      } // End of local variable scope
      break;
    case STATE_DIALING1:
      this->send_cmd_("ATD" + this->recipient_ + ';'); // Dial command, likely same (';' for voice)
      this->state_ = STATE_DIALING2;
      this->expect_ack_ = true; // Expect OK after ATD command
      break;
    case STATE_DIALING2:
      if (ok) {
        ESP_LOGI(TAG, "Dialing: '%s'", this->recipient_.c_str());
        this->dial_pending_ = false;
        // Don't change state yet, wait for call progress (RINGING, CONNECT, NO CARRIER etc in loop)
      } else {
        ESP_LOGE(TAG, "Dial command failed");
        this->dial_pending_ = false;
        this->set_registered_(false); // Assume issue if dial fails
        this->send_cmd_("AT+CMEE=2");
        this->write(26);
      }
      // Go back to INIT to monitor call state with AT+CLCC or wait for URCs
      this->state_ = STATE_INIT;
      break;
    case STATE_PARSE_CLIP:
      // Verify A7672 +CLIP (caller ID) response format
      if (message.compare(0, 6, "+CLIP:") == 0) {
        std::string caller_id = "Unknown";
        size_t start = 7;
        size_t end = message.find(',', start);
        if (end != std::string::npos && end != start) {
           caller_id = message.substr(start + 1, end - start - 2); // Extract number between quotes
        }

        if (this->call_state_ != 4) { // 4 typically means alerting/incoming
          this->call_state_ = 4;
          ESP_LOGI(TAG, "Incoming call from %s", caller_id.c_str());
          incoming_call_callback_.call(caller_id);
        }
        this->state_ = STATE_INIT; // Go back to INIT to monitor call state
      }
      break;
    case STATE_ATA_SENT:
      // Response to ATA (answer call) - needs verification for A7672
      // Often it's just OK, then CONNECT URC later, or directly CONNECT
      if (message == "OK" || message == "CONNECT") {
          ESP_LOGI(TAG, "Call answered (ATA response: %s)", message.c_str());
          // State might change based on +CLCC or other URCs later
          if (this->call_state_ != 0) {
             this->call_state_ = 0; // Assume connected
             this->call_connected_callback_.call();
          }
      } else {
          ESP_LOGW(TAG, "Unexpected response after ATA: %s", message.c_str());
      }
      this->state_ = STATE_INIT; // Go back to INIT
      break;
    default:
      ESP_LOGW(TAG, "Unhandled state: %d, message: %s", this->state_, message.c_str());
      this->state_ = STATE_INIT; // Reset to known state
      break;
  }
}

void A7672Component::loop() { // Renamed class
  while (this->available()) {
    uint8_t byte;
    this->read_byte(&byte);

    if (this->read_pos_ >= A7672_READ_BUFFER_LENGTH) { // Use renamed constant
        ESP_LOGE(TAG, "Read buffer overflow!");
        this->read_pos_ = 0; // Reset buffer position
    }


    ESP_LOGVV(TAG, "Buffer pos: %u Char: %c (0x%02X)", this->read_pos_, (isprint(byte) ? byte : '.'), byte);

    if (byte == ASCII_CR) // Ignore CR
      continue;

    // Handle prompt detection separately?
    if (this->state_ == STATE_SENDING_SMS_2 && byte == '>' && this->read_pos_ == 0) {
        this->read_buffer_[0] = '>';
        this->read_buffer_[1] = 0; // Null terminate
        this->read_pos_ = 0;
        this->parse_cmd_(this->read_buffer_);
        continue; // Skip adding LF processing
    }

    if (byte == ASCII_LF) { // Line feed marks end of a response line
      if (this->read_pos_ > 0) { // Process only if we have data
        this->read_buffer_[this->read_pos_] = 0; // Null terminate
        this->parse_cmd_(this->read_buffer_);
      }
      this->read_pos_ = 0; // Reset buffer position for next line
    } else {
      // Store valid characters
      if (byte >= 0x20 && byte <= 0x7E) { // Store printable ASCII
         this->read_buffer_[this->read_pos_++] = byte;
      } else if (byte != ASCII_CR && byte != ASCII_LF) {
         // Optionally handle or log non-printable chars, but often they are part of complex responses
         // For simplicity, maybe just store them if needed, or log a warning
         // ESP_LOGV(TAG, "Non-printable char ignored: 0x%02X", byte);
         this->read_buffer_[this->read_pos_++] = byte; // Store anyway for now
      }
    }
  }

  // Trigger update() manually if needed (e.g., when pending actions exist)
  // This logic seems okay - if initialized, registered, and something pending, run update()
  if (state_ == STATE_INIT && this->registered_ &&
      (this->call_state_ != 6  // A call is in progress or alerting
       || this->send_pending_ || this->dial_pending_ || this->connect_pending_ || this->disconnect_pending_ || this->send_ussd_pending_)) {
    this->update();
  }
}

void A7672Component::send_sms(const std::string &recipient, const std::string &message) { // Renamed class
  this->recipient_ = recipient;
  this->outgoing_message_ = message;
  this->send_pending_ = true;
  this->update(); // Trigger update to start sending process
}

void A7672Component::send_ussd(const std::string &ussd_code) { // Renamed class
  ESP_LOGD(TAG, "Sending USSD code: %s", ussd_code.c_str());
  this->ussd_ = ussd_code;
  this->send_ussd_pending_ = true;
  this->update(); // Trigger update to start sending process
}
void A7672Component::dump_config() { // Renamed class
  ESP_LOGCONFIG(TAG, "A7672 Component:"); // Renamed component name
#ifdef USE_BINARY_SENSOR
  LOG_BINARY_SENSOR("  ", "Registered", this->registered_binary_sensor_);
#endif
#ifdef USE_SENSOR
  LOG_SENSOR("  ", "RSSI", this->rssi_sensor_); // Keep name 'RSSI' for consistency?
#endif
}
void A7672Component::dial(const std::string &recipient) { // Renamed class
  this->recipient_ = recipient;
  this->dial_pending_ = true;
  this->update(); // Trigger update
}
void A7672Component::connect() { // Renamed class
    this->connect_pending_ = true;
    this->update(); // Trigger update
}
void A7672Component::disconnect() { // Renamed class
    this->disconnect_pending_ = true;
    this->update(); // Trigger update
}

void A7672Component::set_registered_(bool registered) { // Renamed class
  if (this->registered_ != registered) { // Log only on change
     ESP_LOGD(TAG, "Registration status changed: %s", ONOFF(registered));
  }
  this->registered_ = registered;
#ifdef USE_BINARY_SENSOR
  if (this->registered_binary_sensor_ != nullptr)
    this->registered_binary_sensor_->publish_state(registered);
#endif
}

}  // namespace a7672 // Renamed namespace
}  // namespace esphome