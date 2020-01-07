package models

import (
	"errors"
	"strings"
)

type AssetData struct {
	// Family is a group of devices
	Family   string                 `json:"f"`
	Payloads map[string]interface{} `json:"a"`
}

// Validate will validate that the data is okay
func (a *AssetData) Validate() (err error) {
	a.Family = strings.TrimSpace(strings.ToLower(a.Family))

	if a.Family == "" {
		err = errors.New("family cannot be empty")
	}

	numFingerprints := len(a.Payloads)
	if numFingerprints == 0 {
		err = errors.New("asset data cannot be empty")
	}
	return
}
