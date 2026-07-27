if(NOT DEFINED INPUT OR NOT DEFINED OUTPUT)
    message(FATAL_ERROR "INPUTとOUTPUTを指定してください")
endif()

file(READ "${INPUT}" source)

function(replace_required old new)
    string(FIND "${source}" "${old}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "TinyUSB usbh.cの想定箇所が見つかりません: ${old}")
    endif()
    string(REPLACE "${old}" "${new}" source "${source}")
    set(source "${source}" PARENT_SCOPE)
endfunction()

replace_required(
    "static uint8_t _usbh_controller = TUSB_INDEX_INVALID_8;"
    "static uint8_t _usbh_controller_mask = 0;")
replace_required(
    "return _usbh_controller == rhport;"
    "return (_usbh_controller_mask & (uint8_t) (1u << rhport)) != 0;")
replace_required(
    "return _usbh_controller != TUSB_INDEX_INVALID_8;"
    "return _usbh_controller_mask != 0;")
replace_required(
    "_usbh_controller = rhport;"
    "_usbh_controller_mask |= (uint8_t) (1u << rhport);")
replace_required(
    "_usbh_controller = TUSB_INDEX_INVALID_8;"
    "_usbh_controller_mask &= (uint8_t) ~(1u << rhport);")
replace_required(
"  if (enabled) {
    hcd_int_enable(_usbh_controller);
  } else {
    hcd_int_disable(_usbh_controller);
  }"
"  for (uint8_t rhport = 0; rhport < 8; rhport++) {
    if (tuh_rhport_is_active(rhport)) {
      if (enabled) {
        hcd_int_enable(rhport);
      } else {
        hcd_int_disable(rhport);
      }
    }
  }")

file(WRITE "${OUTPUT}" "${source}")
